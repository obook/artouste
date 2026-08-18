/*
 * DemoNavigation.hpp
 * Cap, relèvement et palonnier du pilote automatique de démonstration.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <cmath>

namespace artouste::app::demo_detail {

/* Gains du guidage en cap. */
inline constexpr float GAIN_CAP = 1.4f; /* palonnier par radian d'erreur de cap */
inline constexpr float CAP_MAX  = 0.7f; /* palonnier maximal, évite de pivoter trop vite */

inline float wrapPi(float a) noexcept {
    a = std::fmod(a + PI, TWO_PI);
    if (a < 0.0f) {
        a += TWO_PI;
    }
    return a - PI;
}

/* Cap (rad) d'un vecteur monde horizontal, même convention que l'application :
   atan2(-z, x), donc 0 vers l'est, +PI/2 vers le nord. */
inline float bearing(float dx, float dz) noexcept {
    return std::atan2(-dz, dx);
}

/* Palonnier pour tourner le nez vers la cible (guidage en cap). Un cap visé plus à
   gauche (erreur positive) demande un palonnier négatif, car le palonnier droit
   (positif) fait partir le nez à droite et diminue le cap. */
inline float palonnierVers(const vec3& cible, const vec3& pos, float cap) noexcept {
    const float vise   = bearing(cible.x - pos.x, cible.z - pos.z);
    const float erreur = wrapPi(vise - cap);
    return clamp(-GAIN_CAP * erreur, -CAP_MAX, CAP_MAX);
}

} /* namespace artouste::app::demo_detail */
