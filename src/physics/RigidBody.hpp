/*
 * RigidBody.hpp
 * Corps rigide : tout l'état de mouvement de l'appareil (position, vitesse,
 * orientation par quaternion, vitesse de rotation) et son avancement dans le
 * temps par petits pas réguliers.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::physics {

struct RigidBody {
    vec3 position{0.0f, 0.0f, 0.0f};         /* m, repère monde */
    vec3 velocity{0.0f, 0.0f, 0.0f};         /* m/s, repère monde */
    quat orientation{1.0f, 0.0f, 0.0f, 0.0f};  /* rotation corps -> monde (départ : aucune rotation) */
    vec3 angularVelocity{0.0f, 0.0f, 0.0f};  /* rad/s, repère corps */

    /* Avance d'un pas de durée dt sous l'effet d'une force (en repère monde) et
     * d'un couple (en repère corps). L'inertie est donnée axe par axe (X, Y, Z).
     * Le quaternion est remis à la bonne longueur à chaque pas pour éviter qu'il
     * ne dérive. */
    void integrate(const vec3& forceWorld, const vec3& torqueBody, float mass,
                   const vec3& inertia, float dt) noexcept;
};

}  /* namespace artouste::physics */
