/*
 * ZombieHorde.cpp
 * Voir ZombieHorde.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/ZombieHorde.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace artouste::app {

namespace {
/* Amplitude et fréquence d'un léger balancement vertical du corps, en plus de
   la marche animée par squelette (voir render::SkinnedZombies) : donne un peu
   de vie supplémentaire au déplacement, sans prétendre à un vrai cycle de
   marche (c'est l'animation du modèle qui s'en charge). */
constexpr float IDLE_BOB_AMPLITUDE_M = 0.05f;
constexpr float IDLE_BOB_FREQ_HZ     = 0.6f;
/* Durée de l'anim de chute avant despawn, une fois à 0 PV. */
constexpr float DEATH_ANIM_DURATION_S = 1.0f;
/* Durée du flash de coup touché (décroissance linéaire vers 0). */
constexpr float HIT_FLASH_DURATION_S = 0.15f;

/* Vitesse de marche de base (m/s), avant le facteur de difficulté des vagues
   tardives (voir WaveManager, étape 4). */
constexpr float WALK_SPEED_MS = 1.8f;
/* Portée maximale d'un jet (m, distance horizontale). Hors de portée, un
   zombie continue de marcher vers le joueur sans lancer. */
constexpr float TOXIC_RANGE_MAX_M = 60.0f;
/* Cooldown de jet par zombie (s), tiré aléatoirement dans cet intervalle à
   chaque jet pour désynchroniser la horde (éviter une salve groupée). */
constexpr float THROW_COOLDOWN_MIN_S = 2.0f;
constexpr float THROW_COOLDOWN_MAX_S = 4.0f;
/* Hauteur (m) à laquelle part la boulette, à peu près celle des mains d'un
   zombie qui lance le bras en avant. */
constexpr float THROW_ORIGIN_HEIGHT_M = 1.4f;

/* Position des yeux dans le repère local du modèle, dont le chargeur normalise
   la taille à 1,80 m (voir render::SkinnedModel, TARGET_HEIGHT_M) : hauteur du
   regard, demi-écart entre les deux yeux et avancée sur le visage. Le modèle
   regarde vers +Z local, sens dans lequel buildInstanceMatrices oriente la
   marche. */
constexpr float EYE_HEIGHT_M  = 1.62f;
constexpr float EYE_SPACING_M = 0.075f;
constexpr float EYE_FORWARD_M = 0.11f;
/* Rayon de base de la lueur (m). Le shader la fait grossir avec la distance
   pour qu'elle reste repérable depuis l'hélicoptère (voir zombie_eyes.vert) :
   cette valeur ne vaut donc que de tout près. */
constexpr float EYE_RADIUS_M = 0.13f;

/* Couleurs des lueurs, au-delà de 1 pour saturer franchement le rendu additif :
   vert pour un marcheur, rouge pour une pondeuse (le boss se repère ainsi de
   loin, avant même de distinguer sa silhouette). */
const vec3 EYE_COLOR_WALKER{0.30f, 3.00f, 0.50f};
const vec3 EYE_COLOR_BROOD{3.20f, 0.18f, 0.10f};
}  /* namespace */

void ZombieHorde::spawn(const vec3& position, float yaw, float phase) {
    Zombie z;
    z.position = position;
    z.yaw      = yaw;
    z.phase    = phase;
    z.kind     = static_cast<unsigned int>(m_rng());  /* variante + groupe de phase déduits au rendu */
    m_zombies.push_back(z);
}

void ZombieHorde::spawnBrood(const vec3& position, float yaw, float phase) {
    spawn(position, yaw, phase);
    Zombie& z = m_zombies.back();
    z.type    = Type::Brood;
    z.scale   = BROOD_SCALE;
    z.health  = BROOD_HEALTH;
}

bool ZombieHorde::broodAlive() const noexcept {
    return std::any_of(m_zombies.begin(), m_zombies.end(), [](const Zombie& z) {
        return z.type == Type::Brood && z.state == State::Alive;
    });
}

vec3 ZombieHorde::broodPosition() const noexcept {
    for (const Zombie& z : m_zombies) {
        if (z.type == Type::Brood && z.state == State::Alive) {
            return z.position;
        }
    }
    return vec3{0.0f};
}

float ZombieHorde::broodHealthPct() const noexcept {
    for (const Zombie& z : m_zombies) {
        if (z.type == Type::Brood && z.state == State::Alive) {
            return saturate(z.health / BROOD_HEALTH);
        }
    }
    return 0.0f;
}

void ZombieHorde::applyDamage(std::size_t index, float amount) noexcept {
    if (index >= m_zombies.size()) {
        return;
    }
    Zombie& z = m_zombies[index];
    if (z.state != State::Alive) {
        return;
    }
    z.hitFlashTimer = HIT_FLASH_DURATION_S;
    z.health -= amount;
    if (z.health <= 0.0f) {
        z.health     = 0.0f;
        z.state      = State::Dying;
        z.stateTimer = DEATH_ANIM_DURATION_S;
    }
}

void ZombieHorde::snapToGround(const std::function<float(float, float)>& terrainHeight) noexcept {
    if (!terrainHeight) {
        return;
    }
    for (Zombie& z : m_zombies) {
        z.position.y = terrainHeight(z.position.x, z.position.z);
    }
}

std::vector<ThrowRequest> ZombieHorde::update(float dt, const vec3& playerPos, float playerAgl,
                                              float speedFactor,
                                              const std::function<float(float, float)>& terrainHeight) noexcept {
    m_time += dt;
    std::vector<ThrowRequest> requests;
    std::uniform_real_distribution<float> cooldownDist(THROW_COOLDOWN_MIN_S, THROW_COOLDOWN_MAX_S);

    for (Zombie& z : m_zombies) {
        z.hitFlashTimer = std::max(0.0f, z.hitFlashTimer - dt);

        if (z.state == State::Dying) {
            z.stateTimer -= dt;
            if (z.stateTimer <= 0.0f) {
                z.state = State::Dead;
            }
            continue;  /* un zombie qui tombe ne marche plus et ne lance plus */
        }
        if (z.state != State::Alive) {
            continue;
        }

        /* Marche en ligne droite vers le joueur (plan horizontal), sans
           pathfinding -- suffisant sur un terrain dégagé. */
        const vec3  toPlayer = vec3{playerPos.x - z.position.x, 0.0f, playerPos.z - z.position.z};
        const float dist     = glm::length(toPlayer);
        if (dist > 0.01f) {
            const vec3  dir   = toPlayer / dist;
            const float speed = WALK_SPEED_MS * speedFactor *
                                (z.type == Type::Brood ? BROOD_SPEED_FACTOR : 1.0f);
            const float step  = std::min(speed * dt, dist);
            z.position.x += dir.x * step;
            z.position.z += dir.z * step;
            z.yaw = std::atan2(dir.x, dir.z);
        }

        /* Attaque à distance : à portée, hors cooldown, et le joueur sous le
           plafond d'altitude -- au-dessus, les zombies restent visibles et
           continuent de converger, mais ne peuvent pas viser. */
        z.throwCooldownS = std::max(0.0f, z.throwCooldownS - dt);
        if (dist <= TOXIC_RANGE_MAX_M && playerAgl <= TOXIC_CEILING_M && z.throwCooldownS <= 0.0f) {
            /* Hauteur de bras mise à l'échelle du lanceur : sans cela, une
               pondeuse de près de six mètres lancerait depuis ses chevilles. */
            requests.push_back(
                ThrowRequest{z.position + vec3{0.0f, THROW_ORIGIN_HEIGHT_M * z.scale, 0.0f},
                             playerPos});
            /* Cooldown désynchronisé, raccourci par le facteur de difficulté
               (vagues tardives : jets plus fréquents, pas seulement plus
               rapides à marcher). */
            z.throwCooldownS = cooldownDist(m_rng) / std::max(speedFactor, 0.01f);
        }
    }

    m_zombies.erase(std::remove_if(m_zombies.begin(), m_zombies.end(),
                                   [](const Zombie& z) { return z.state == State::Dead; }),
                    m_zombies.end());

    snapToGround(terrainHeight);
    return requests;
}

mat4 ZombieHorde::instanceMatrix(const Zombie& z) const {
    mat4 m;
    if (z.state == State::Dying) {
        /* Bascule progressivement en avant (chute) : anim procédurale simple,
           pas de ragdoll physique -- suffisant pour un zombie qui despawn
           peu après (voir DEATH_ANIM_DURATION_S). */
        const float progress = 1.0f - saturate(z.stateTimer / DEATH_ANIM_DURATION_S);
        m = glm::translate(mat4(1.0f), z.position - vec3{0.0f, progress * 0.4f, 0.0f});
        m = glm::rotate(m, z.yaw, vec3{0.0f, 1.0f, 0.0f});
        m = glm::rotate(m, progress * HALF_PI, vec3{1.0f, 0.0f, 0.0f});
    } else {
        const float bob =
            std::sin(m_time * IDLE_BOB_FREQ_HZ * TWO_PI + z.phase) * IDLE_BOB_AMPLITUDE_M;
        m = glm::translate(mat4(1.0f), z.position + vec3{0.0f, bob, 0.0f});
        m = glm::rotate(m, z.yaw, vec3{0.0f, 1.0f, 0.0f});
    }
    if (z.scale != 1.0f) {
        m = glm::scale(m, vec3{z.scale});  /* pondeuse : même modèle, agrandi */
    }
    return m;
}

std::vector<mat4> ZombieHorde::buildInstanceMatrices() const {
    std::vector<mat4> out;
    out.reserve(m_zombies.size());
    for (const Zombie& z : m_zombies) {
        if (z.state == State::Dead) {
            continue;
        }
        out.push_back(instanceMatrix(z));
    }
    return out;
}

std::vector<ZombieHorde::EyeView> ZombieHorde::buildEyes() const {
    std::vector<EyeView> out;
    out.reserve(m_zombies.size() * 2);
    for (const Zombie& z : m_zombies) {
        if (z.state == State::Dead) {
            continue;
        }
        /* Le regard s'éteint pendant la chute : la lueur suit la disparition du
           zombie au lieu de rester allumée sur un corps à terre. */
        const float fade = z.state == State::Dying ? saturate(z.stateTimer / DEATH_ANIM_DURATION_S)
                                                   : 1.0f;
        if (fade <= 0.0f) {
            continue;
        }
        const vec3 color = (z.type == Type::Brood ? EYE_COLOR_BROOD : EYE_COLOR_WALKER) * fade;
        const mat4 m     = instanceMatrix(z);
        for (const float side : {-1.0f, 1.0f}) {
            const vec4 local{side * EYE_SPACING_M, EYE_HEIGHT_M, EYE_FORWARD_M, 1.0f};
            EyeView    eye;
            eye.position = vec3(m * local);
            eye.radius   = EYE_RADIUS_M * z.scale;
            eye.color    = color;
            out.push_back(eye);
        }
    }
    return out;
}

std::vector<float> ZombieHorde::buildHitFlashes() const {
    std::vector<float> out;
    out.reserve(m_zombies.size());
    for (const Zombie& z : m_zombies) {
        if (z.state == State::Dead) {
            continue;
        }
        out.push_back(saturate(z.hitFlashTimer / HIT_FLASH_DURATION_S));
    }
    return out;
}

std::vector<vec3> ZombieHorde::alivePositions() const {
    std::vector<vec3> out;
    out.reserve(m_zombies.size());
    for (const Zombie& z : m_zombies) {
        if (z.state != State::Dead) {
            out.push_back(z.position);
        }
    }
    return out;
}

std::vector<int> ZombieHorde::buildKinds() const {
    std::vector<int> out;
    out.reserve(m_zombies.size());
    for (const Zombie& z : m_zombies) {
        if (z.state == State::Dead) {
            continue;  /* même filtrage que buildInstanceMatrices */
        }
        out.push_back(static_cast<int>(z.kind));
    }
    return out;
}

}  /* namespace artouste::app */
