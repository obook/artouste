/*
 * RigidBody.cpp
 * Avancement du corps rigide d'un pas de temps : on applique les lois de Newton
 * pour mettre à jour la vitesse, la position, puis la rotation.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "physics/RigidBody.hpp"

namespace artouste::physics {

void RigidBody::integrate(const vec3& forceWorld, const vec3& torqueBody, float mass,
                          const vec3& inertia, float dt) noexcept {
    /* Déplacement : on calcule l'accélération (F = m.a), on met à jour la vitesse,
     * puis la position avec cette nouvelle vitesse. Faire la vitesse d'abord rend
     * le calcul plus stable. */
    const vec3 acceleration = forceWorld / mass;
    velocity += acceleration * dt;
    position += velocity * dt;

    /* Rotation : le couple divisé par l'inertie donne l'accélération de rotation,
     * d'où la nouvelle vitesse de rotation. */
    const vec3 angularAcceleration = torqueBody / inertia;
    angularVelocity += angularAcceleration * dt;

    /* Mise à jour de l'orientation à partir de la vitesse de rotation. La formule
     * dq/dt = 0.5 * q * (0, omega) fait tourner le quaternion. On le renormalise
     * pour qu'il reste une rotation valide. */
    const quat spin{0.0f, angularVelocity.x, angularVelocity.y, angularVelocity.z};
    orientation += 0.5f * (orientation * spin) * dt;
    orientation = glm::normalize(orientation);
}

}  /* namespace artouste::physics */
