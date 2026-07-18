/*
 * ProjectileSystem.hpp
 * Boulettes toxiques lancées par les zombies (voir ZombieHorde::ThrowRequest) :
 * trajectoire parabolique simple (pas de moteur physique complet), impact
 * détecté avec le même utilitaire que la mitrailleuse (physics::raySphere),
 * appliqué au segment parcouru en une frame plutôt qu'à un rayon instantané.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <cstddef>
#include <vector>

namespace artouste::app {

class ProjectileSystem {
public:
    /* Fait partir une boulette de origin vers target (position du joueur au
       moment du jet -- pas de guidage ensuite) : vitesse initiale calculée
       par formule fermée pour une trajectoire parabolique de durée fixe,
       proportionnelle à la distance. Sans effet au-delà de MAX_PROJECTILES
       (filet de sécurité, ne devrait pas arriver en pratique). */
    void spawn(const vec3& origin, const vec3& target) noexcept;

    /* Avance chaque boulette d'un pas de temps (gravité + position), teste
       l'impact du segment parcouru contre la sphère de l'appareil
       (heliCenter, heliRadius), retire les boulettes qui touchent ou
       expirent (durée de vie ou chute au sol). Renvoie les dégâts totaux à
       appliquer ce pas de temps (0 si aucun impact). */
    float update(float dt, const vec3& heliCenter, float heliRadius) noexcept;

    /* Instances courantes (xyz = centre monde, w = échelle), prêtes pour
       render::Projectiles::updateInstances. */
    [[nodiscard]] std::vector<vec4> buildInstances() const;

    [[nodiscard]] std::size_t count() const noexcept { return m_projectiles.size(); }

    /* Vide toutes les boulettes en vol (redémarrage d'une partie) : sans quoi
       les boulettes d'une session précédente restent affichées ("pluie de
       boulettes" au relancement du mode zombie). */
    void clear() noexcept { m_projectiles.clear(); }

private:
    struct Projectile {
        vec3  position{0.0f};
        vec3  velocity{0.0f};
        float lifetimeS = 0.0f;  /* despawn de sécurité si rien n'est touché */
    };

    std::vector<Projectile> m_projectiles;
};

}  /* namespace artouste::app */
