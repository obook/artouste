/*
 * ProjectileSystem.cpp
 * Voir ProjectileSystem.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/ProjectileSystem.hpp"

#include "physics/Raycast.hpp"

#include <algorithm>
#include <cmath>

namespace artouste::app {

namespace {
/* Capacité de sécurité : au-delà, on ignore les nouveaux jets plutôt que de
   laisser le nombre de boulettes actives dériver sans borne. Largement plus
   que ce qu'une horde plafonnée (voir ZOMBIE_CAPACITY, ApplicationScene.cpp)
   peut produire à la fois. */
constexpr std::size_t MAX_PROJECTILES = 64;

/* Gravité de la trajectoire (m/s^2), volontairement plus faible que 9,81
   pour un arc plus lisible et esquivable (aide de jeu, pas une simulation
   réaliste de lancer). */
constexpr float GRAVITY_TOXIC = 6.0f;
/* Vitesse horizontale de référence (m/s) : sert à dériver une durée de vol
   fixe, proportionnelle à la distance, plutôt qu'une vitesse initiale fixe
   (qui donnerait un arc différent selon la distance). */
constexpr float THROW_SPEED_XZ = 14.0f;
/* Durée de vol plancher (s), pour éviter une division quasi nulle à très
   courte distance (le zombie est presque au contact). */
constexpr float MIN_FLIGHT_TIME_S = 0.4f;
/* Despawn de sécurité si rien n'est touché (trajectoire manquée, appareil
   qui s'est éloigné entre le jet et l'impact prévu). */
constexpr float MAX_LIFETIME_S = 4.0f;
/* Échelle (diamètre, m) du sprite billboard de la boulette. */
constexpr float PROJECTILE_SCALE = 0.4f;
/* Dégâts infligés au joueur par impact. */
constexpr float PROJECTILE_DAMAGE = 8.0f;
}  /* namespace */

void ProjectileSystem::spawn(const vec3& origin, const vec3& target) noexcept {
    if (m_projectiles.size() >= MAX_PROJECTILES) {
        return;
    }
    const vec3  delta      = target - origin;
    const float horizDist  = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    const float flightTime = std::max(horizDist / THROW_SPEED_XZ, MIN_FLIGHT_TIME_S);

    Projectile p;
    p.position  = origin;
    p.velocity  = vec3{delta.x / flightTime,
                       delta.y / flightTime + 0.5f * GRAVITY_TOXIC * flightTime,
                       delta.z / flightTime};
    p.lifetimeS = MAX_LIFETIME_S;
    m_projectiles.push_back(p);
}

float ProjectileSystem::update(float dt, const vec3& heliCenter, float heliRadius) noexcept {
    float damage = 0.0f;

    for (auto it = m_projectiles.begin(); it != m_projectiles.end();) {
        Projectile& p       = *it;
        const vec3  prevPos = p.position;
        p.velocity.y -= GRAVITY_TOXIC * dt;
        p.position += p.velocity * dt;
        p.lifetimeS -= dt;

        /* Impact : segment parcouru cette frame contre la sphère de
           l'appareil, même utilitaire que la mitrailleuse (voir Weapon), en
           sens inverse (c'est ici le projectile qui se déplace, pas la
           cible). */
        bool        impact = false;
        const vec3  segment = p.position - prevPos;
        const float segLen  = glm::length(segment);
        if (segLen > 0.0001f) {
            const vec3                  dir = segment / segLen;
            const physics::RaySphereHit hit = physics::raySphere(prevPos, dir, heliCenter, heliRadius);
            impact                          = hit.hit && hit.distance <= segLen;
        }

        if (impact) {
            damage += PROJECTILE_DAMAGE;
            it = m_projectiles.erase(it);
        } else if (p.lifetimeS <= 0.0f) {
            it = m_projectiles.erase(it);
        } else {
            ++it;
        }
    }

    return damage;
}

std::vector<vec4> ProjectileSystem::buildInstances() const {
    std::vector<vec4> out;
    out.reserve(m_projectiles.size());
    for (const Projectile& p : m_projectiles) {
        out.emplace_back(p.position, PROJECTILE_SCALE);
    }
    return out;
}

}  /* namespace artouste::app */
