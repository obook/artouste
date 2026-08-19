/*
 * Raycast.hpp
 * Intersection rayon/sphère, utilitaire géométrique générique et isolé (pas
 * de dépendance au jeu). Sert à la fois à la mitrailleuse (rayon instantané
 * du canon vers chaque zombie) et aux pneus toxiques (segment parcouru en
 * une frame contre la sphère de l'hélicoptère) -- voir app::Weapon et
 * app::ProjectileSystem.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::physics {

struct RaySphereHit {
    bool  hit      = false;
    float distance = 0.0f;  /* le long de dir (déjà normalisé), depuis origin */
};

/* Intersection la plus proche, devant l'origine, entre le rayon (origin, dir)
   et la sphère (sphereCenter, sphereRadius). dir DOIT être normalisé. */
[[nodiscard]] RaySphereHit raySphere(const vec3& origin, const vec3& dir,
                                     const vec3& sphereCenter, float sphereRadius) noexcept;

}  /* namespace artouste::physics */
