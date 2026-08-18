/*
 * ZombieHorde.cpp
 * Voir ZombieHorde.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/ZombieHorde.hpp"

#include "app/combat/ZombieHordeReglages.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace artouste::app {

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

void ZombieHorde::spawnBroodling(const vec3& position, float yaw, float phase) {
    spawn(position, yaw, phase);
    m_zombies.back().fromBrood = true;
}

std::vector<vec3> ZombieHorde::killBroodlings() noexcept {
    std::vector<vec3> positions;
    for (Zombie& z : m_zombies) {
        if (!z.fromBrood || z.state != State::Alive) {
            continue;
        }
        /* Mise à mort immédiate, sans passer par applyDamage : ils n'encaissent
           pas un coup, ils s'éteignent avec ce qui les a lâchés. L'animation de
           chute reste jouée (Dying), le temps que l'explosion les couvre. */
        z.health     = 0.0f;
        z.state      = State::Dying;
        z.stateTimer = DEATH_ANIM_DURATION_S;
        positions.push_back(z.position);
    }
    return positions;
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
            /* Hauteur de bras mise à l'échelle du lanceur : sans cela, un
               largueur de près de six mètres lancerait depuis ses chevilles. */
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

} /* namespace artouste::app */
