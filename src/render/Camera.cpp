/*
 * Camera.cpp
 * Calcule la position de la caméra et les matrices de vue et de
 * projection selon le mode choisi (orbite, poursuite ou cockpit).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/Camera.hpp"

namespace artouste::render {

void Camera::orbit(const vec3& target, float radius, float height, float angleRad) noexcept {
    m_target   = target;
    m_up       = vec3{0.0f, 1.0f, 0.0f};
    m_position = target + vec3{radius * std::cos(angleRad), height, radius * std::sin(angleRad)};
}

void Camera::chase(const vec3& target, float yaw, float dt) noexcept {
    constexpr float CHASE_BACK   = 9.0f;   /* m derrière l'appareil */
    constexpr float CHASE_UP     = 3.5f;   /* m au-dessus */
    constexpr float LOOK_UP      = 1.5f;   /* point visé au-dessus du centre */
    constexpr float YAW_TAU      = 0.40f;  /* retard du cap (horizon stable) */
    constexpr float POS_TAU      = 0.15f;  /* retard de position */
    constexpr float MIN_HEIGHT   = 0.8f;   /* la caméra ne traverse pas le sol */

    if (!m_followInit) {
        m_followYaw  = yaw;
        m_followInit = true;
    }
    m_followYaw = lowPassAngle(m_followYaw, yaw, dt, YAW_TAU);

    /* Direction "avant" de l'appareil dans le monde (à partir du cap lisse). */
    const vec3 forward{std::cos(m_followYaw), 0.0f, -std::sin(m_followYaw)};
    vec3 desiredEye = target - forward * CHASE_BACK + vec3{0.0f, CHASE_UP, 0.0f};
    if (desiredEye.y < MIN_HEIGHT) {
        desiredEye.y = MIN_HEIGHT;
    }

    if (m_position == vec3{0.0f, 0.0f, 0.0f}) {
        m_position = desiredEye;  /* première image : on évite un glissement depuis l'origine */
    }
    m_up       = vec3{0.0f, 1.0f, 0.0f};
    m_position = lowPass(m_position, desiredEye, dt, POS_TAU);
    m_target   = lowPass(m_target, target + vec3{0.0f, LOOK_UP, 0.0f}, dt, POS_TAU);
}

void Camera::setLookAt(const vec3& eye, const vec3& target, const vec3& up) noexcept {
    m_position = eye;
    m_target   = target;
    m_up       = up;
}

mat4 Camera::view() const noexcept {
    return glm::lookAt(m_position, m_target, m_up);
}

mat4 Camera::proj() const noexcept {
    return glm::perspective(m_fovY, m_aspect, m_near, m_far);
}

}  /* namespace artouste::render */
