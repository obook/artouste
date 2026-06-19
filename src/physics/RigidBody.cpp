#include "physics/RigidBody.hpp"

namespace artouste::physics {

void RigidBody::integrate(const vec3& forceWorld, const vec3& torqueBody, float mass,
                          const vec3& inertia, float dt) noexcept {
    // Translation : Euler semi-implicite (on met à jour la vitesse avant la
    // position, plus stable que l'Euler explicite).
    const vec3 acceleration = forceWorld / mass;
    velocity += acceleration * dt;
    position += velocity * dt;

    // Rotation : couple corps -> accélération angulaire (inertie diagonale).
    const vec3 angularAcceleration = torqueBody / inertia;
    angularVelocity += angularAcceleration * dt;

    // Intégration du quaternion : dq/dt = 0.5 * q * (0, omega_corps).
    const quat spin{0.0f, angularVelocity.x, angularVelocity.y, angularVelocity.z};
    orientation += 0.5f * (orientation * spin) * dt;
    orientation = glm::normalize(orientation);
}

}  // namespace artouste::physics
