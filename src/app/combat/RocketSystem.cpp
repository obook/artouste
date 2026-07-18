/*
 * RocketSystem.cpp
 * Voir RocketSystem.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/RocketSystem.hpp"

#include "physics/Raycast.hpp"

#include <algorithm>
#include <cmath>

namespace artouste::app {

namespace {
/* Capacité de sécurité : au-delà, on ignore les nouveaux tirs plutôt que de
   laisser le nombre de roquettes en vol dériver sans borne. */
constexpr std::size_t MAX_ROCKETS = 64;

/* Vitesse de la roquette (m/s) : rapide, mais assez lente pour se voir voler
   du canon jusqu'au sol. */
constexpr float ROCKET_SPEED_MS = 130.0f;
/* Gravité appliquée à la roquette (m/s^2) : faible devant la vitesse, juste de
   quoi la faire retomber et exploser au sol même tirée à peu près à plat. */
constexpr float ROCKET_GRAVITY = 12.0f;
/* Despawn de sécurité (s) si la roquette ne touche ni le sol ni un zombie
   (tir vers le ciel) : elle finit par exploser en l'air, sans dégâts utiles. */
constexpr float ROCKET_LIFETIME_S = 5.0f;
/* Longueur de la traînée de feu affichée derrière la roquette (m). */
constexpr float ROCKET_TRAIL_M = 5.0f;

/* Rayon de contact direct roquette/zombie en vol (m) : une roquette qui frôle
   un zombie explose sur lui plutôt que de le traverser. */
constexpr float DIRECT_HIT_RADIUS_M = 1.2f;
/* Hauteur du centre du zombie visé pour ce test de contact (m au-dessus du
   sol), même logique que la sphère de collision de la mitrailleuse d'avant. */
constexpr float DIRECT_HIT_HEIGHT_M = 0.9f;
/* Dégâts appliqués à chaque zombie dans le rayon d'explosion : très au-delà de
   la vie (100) pour une mise à mort certaine en zone. */
constexpr float BLAST_DAMAGE = 1000.0f;

/* Durée d'affichage de l'explosion (s) : progress parcourt 0->1 sur cette durée.
   DOIT valoir la tranche d'animation jouée par render::ExplosionFx (playSpanS,
   voir ApplicationScene) pour une lecture a vitesse réelle du flipbook -- sinon
   les images discretes du pack "sautent". On ne joue qu'une partie de l'anim
   (3 s au total) pour un impact punchy. */
constexpr float EXPLOSION_DURATION_S = 1.2f;

/* Durée de vie d'une trace de brûlure au sol (s) : s'estompe progressivement. */
constexpr float SCORCH_DURATION_S = 300.0f;  /* 5 minutes */
/* Filet de sécurité : au-dela, on retire la plus ancienne trace. Un tir
   quasi continu pendant les 5 minutes de vie d'une trace produirait bien plus
   d'impacts que ce plafond (des centaines) ; dans ce cas les plus anciennes
   disparaissent avant terme plutôt que de multiplier les décalques à l'écran. */
constexpr std::size_t MAX_SCORCHES = 400;
}  /* namespace */

void RocketSystem::spawn(const vec3& origin, const vec3& dir) noexcept {
    if (m_rockets.size() >= MAX_ROCKETS) {
        return;
    }
    Rocket r;
    r.position  = origin;
    r.velocity  = dir * ROCKET_SPEED_MS;
    r.lifetimeS = ROCKET_LIFETIME_S;
    m_rockets.push_back(r);
}

RocketSystem::UpdateResult RocketSystem::update(
    float dt, const std::function<float(float, float)>& terrainHeight, ZombieHorde& horde) noexcept {
    UpdateResult res;

    for (auto it = m_rockets.begin(); it != m_rockets.end();) {
        Rocket&    r       = *it;
        const vec3 prevPos = r.position;
        r.velocity.y -= ROCKET_GRAVITY * dt;
        r.position += r.velocity * dt;
        r.lifetimeS -= dt;

        /* Point de détonation, déterminé par ordre de priorité : sol franchi,
           puis contact direct d'un zombie sur le trajet, puis expiration. */
        bool detonate = false;
        vec3 center   = r.position;

        const float ground = terrainHeight(r.position.x, r.position.z);
        if (r.position.y <= ground) {
            detonate = true;
            center   = vec3{r.position.x, ground, r.position.z};
        }

        if (!detonate) {
            const vec3  seg    = r.position - prevPos;
            const float segLen = glm::length(seg);
            if (segLen > 1e-4f) {
                const vec3                        dir     = seg / segLen;
                const std::vector<ZombieHorde::Zombie>& zombies = horde.zombies();
                for (const ZombieHorde::Zombie& z : zombies) {
                    if (z.state != ZombieHorde::State::Alive) {
                        continue;
                    }
                    const vec3 c = z.position + vec3{0.0f, DIRECT_HIT_HEIGHT_M, 0.0f};
                    const physics::RaySphereHit hit =
                        physics::raySphere(prevPos, dir, c, DIRECT_HIT_RADIUS_M);
                    if (hit.hit && hit.distance <= segLen) {
                        detonate = true;
                        center   = prevPos + dir * hit.distance;
                        break;
                    }
                }
            }
        }

        if (!detonate && r.lifetimeS <= 0.0f) {
            detonate = true;
            center   = r.position;
        }

        if (!detonate) {
            ++it;
            continue;
        }

        /* Explosion toujours ramenée au sol (le tir "provoque au sol une
           explosion") : même sur un contact direct en l'air, la boule de feu
           et sa zone létale sont calées au niveau du terrain sous le point
           d'impact, pour une portée horizontale cohérente contre des zombies
           tous au sol. */
        center.y = terrainHeight(center.x, center.z);

        /* Boule de feu affichée + dégâts de zone. Tout zombie vivant dont le
           centre est dans le rayon est tué (dégâts largement létaux) ; on
           compte les mises à mort pour le son. */
        ++res.explosions;
        m_explosions.push_back(Explosion{center, 0.0f});
        res.explosionPositions.push_back(center);
        /* Trace de brûlure persistante au sol (s'estompe en ~45 s). */
        if (m_scorches.size() >= MAX_SCORCHES) {
            m_scorches.erase(m_scorches.begin());  /* retire la plus ancienne */
        }
        m_scorches.push_back(Scorch{center, 0.0f});

        int                                killsHere = 0;
        std::vector<ZombieHorde::Zombie>& zombies   = horde.zombies();
        for (std::size_t i = 0; i < zombies.size(); ++i) {
            if (zombies[i].state != ZombieHorde::State::Alive) {
                continue;
            }
            if (glm::distance(zombies[i].position, center) <= EXPLOSION_RADIUS_M) {
                const vec3 zombiePos = zombies[i].position;
                horde.applyDamage(i, BLAST_DAMAGE);
                if (zombies[i].state == ZombieHorde::State::Dying) {
                    ++res.kills;
                    ++killsHere;
                    res.zombieDeathPositions.push_back(zombiePos);
                } else {
                    res.zombieHitPositions.push_back(zombiePos);
                }
            }
        }
        res.explosionKillCounts.push_back(killsHere);

        it = m_rockets.erase(it);
    }

    /* Vieillissement des boules de feu. */
    for (Explosion& e : m_explosions) {
        e.age += dt;
    }
    m_explosions.erase(std::remove_if(m_explosions.begin(), m_explosions.end(),
                                      [](const Explosion& e) { return e.age >= EXPLOSION_DURATION_S; }),
                       m_explosions.end());

    /* Vieillissement des traces au sol. */
    for (Scorch& s : m_scorches) {
        s.age += dt;
    }
    m_scorches.erase(std::remove_if(m_scorches.begin(), m_scorches.end(),
                                    [](const Scorch& s) { return s.age >= SCORCH_DURATION_S; }),
                     m_scorches.end());

    return res;
}

std::vector<RocketSystem::RocketView> RocketSystem::rockets() const {
    std::vector<RocketView> out;
    out.reserve(m_rockets.size());
    for (const Rocket& r : m_rockets) {
        const float speed = glm::length(r.velocity);
        const vec3  dir   = speed > 1e-4f ? r.velocity / speed : vec3{1.0f, 0.0f, 0.0f};
        out.push_back(RocketView{r.position, r.position - dir * ROCKET_TRAIL_M});
    }
    return out;
}

std::vector<RocketSystem::ExplosionView> RocketSystem::explosions() const {
    std::vector<ExplosionView> out;
    out.reserve(m_explosions.size());
    for (const Explosion& e : m_explosions) {
        ExplosionView v;
        v.center   = e.center;
        v.progress = std::min(1.0f, e.age / EXPLOSION_DURATION_S);
        out.push_back(v);
    }
    return out;
}

std::vector<RocketSystem::ScorchView> RocketSystem::scorches() const {
    std::vector<ScorchView> out;
    out.reserve(m_scorches.size());
    for (const Scorch& s : m_scorches) {
        ScorchView v;
        v.center = s.center;
        /* Pleine au début puis fondu linéaire jusqu'a 0 sur la durée de vie. */
        v.alpha  = 1.0f - s.age / SCORCH_DURATION_S;
        out.push_back(v);
    }
    return out;
}

}  /* namespace artouste::app */
