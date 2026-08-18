/*
 * RocketSystem.cpp
 * Voir RocketSystem.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/RocketSystem.hpp"

#include "app/combat/RocketSystemReglages.hpp"

#include "physics/Raycast.hpp"

#include <algorithm>
#include <cmath>

namespace artouste::app {



void RocketSystem::spawn(const vec3& origin, const vec3& dir) noexcept {
    if (m_rockets.size() >= MAX_ROCKETS) {
        return;
    }
    Rocket r;
    r.position  = origin;
    r.velocity  = dir * ROCKET_SPEED_MS;
    r.origin    = origin;
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
                    /* Sphère mise à l'échelle du zombie : un largueur
                       (ZombieHorde::BROOD_SCALE) est une cible plus haute et
                       plus large, comme sa silhouette le laisse attendre. */
                    const vec3 c = z.position + vec3{0.0f, DIRECT_HIT_HEIGHT_M * z.scale, 0.0f};
                    const physics::RaySphereHit hit =
                        physics::raySphere(prevPos, dir, c, DIRECT_HIT_RADIUS_M * z.scale);
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
        /* Trace de brûlure persistante au sol (s'estompe en ~45 s), de forme et
           de taille propres à cet impact : angle d'arrivée et portée du tir
           (voir scorchShapeFor). Figées ici une fois pour toutes, la roquette
           n'existant plus ensuite. */
        if (m_scorches.size() >= MAX_SCORCHES) {
            m_scorches.erase(m_scorches.begin());  /* retire la plus ancienne */
        }
        const float rangeM =
            glm::length(vec2{center.x - r.origin.x, center.z - r.origin.z});
        const ScorchShape shape = scorchShapeFor(r.velocity, rangeM);
        m_scorches.push_back(Scorch{center, 0.0f, shape.radius, shape.elongation, shape.yaw});

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

} /* namespace artouste::app */
