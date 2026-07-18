/*
 * Raycast.cpp
 * Voir Raycast.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "physics/Raycast.hpp"

#include <cmath>

namespace artouste::physics {

RaySphereHit raySphere(const vec3& origin, const vec3& dir, const vec3& sphereCenter,
                       float sphereRadius) noexcept {
    const vec3  oc            = origin - sphereCenter;
    const float b             = glm::dot(oc, dir);
    const float c             = glm::dot(oc, oc) - sphereRadius * sphereRadius;
    const float discriminant  = b * b - c;
    if (discriminant < 0.0f) {
        return {};  /* pas d'intersection */
    }
    const float sqrtD = std::sqrt(discriminant);
    /* Racine la plus proche d'abord ; si elle est derrière l'origine (sphère
       chevauchant l'origine), on retient l'autre. */
    float t = -b - sqrtD;
    if (t < 0.0f) {
        t = -b + sqrtD;
    }
    if (t < 0.0f) {
        return {};  /* la sphère est entièrement derrière l'origine */
    }
    return RaySphereHit{true, t};
}

}  /* namespace artouste::physics */
