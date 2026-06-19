// Corps rigide : état cinématique complet (position, vitesse, orientation par
// quaternion, vitesse angulaire) et intégration Euler semi-implicite à pas fixe.
// Aucune dépendance au rendu : seules les maths GLM sont utilisées.

#pragma once

#include "util/Math.hpp"

namespace artouste::physics {

struct RigidBody {
    vec3 position{0.0f, 0.0f, 0.0f};         // m, repère monde
    vec3 velocity{0.0f, 0.0f, 0.0f};         // m/s, repère monde
    quat orientation{1.0f, 0.0f, 0.0f, 0.0f};  // corps -> monde (identité)
    vec3 angularVelocity{0.0f, 0.0f, 0.0f};  // rad/s, repère corps

    // Avance d'un pas dt sous une force (monde) et un couple (corps). L'inertie
    // est diagonale : composantes autour de X, Y, Z. Le quaternion est
    // renormalisé à chaque pas.
    void integrate(const vec3& forceWorld, const vec3& torqueBody, float mass,
                   const vec3& inertia, float dt) noexcept;
};

}  // namespace artouste::physics
