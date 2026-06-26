/*
 * Camera.cpp
 * Calcule la position de la caméra et les matrices de vue et de
 * projection selon le mode choisi (orbite, poursuite ou cockpit).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Camera.hpp"

namespace artouste::render {

void Camera::orbitSolar(const vec3& target, const vec3& sunDir, float radius, float height) noexcept
{
    m_up = vec3{0.0f, 1.0f, 0.0f};

    // securite / 0 sui soleil au zenit
    vec3 safeSunDir = sunDir;
    if (std::abs(safeSunDir.y) > 0.99f)
    {
        safeSunDir.x += 0.01f;
        safeSunDir = glm::normalize(safeSunDir);
    }

    //cadrage soleil
    float sunElev = std::max(0.0f, safeSunDir.y);
    float sunPitch = std::max(0.0f, std::atan2(safeSunDir.y, glm::length(vec2{safeSunDir.x, safeSunDir.z})));
    float currentRadius = radius + (sunPitch * 12.0f);//recule un peu
    
    m_target = target + vec3{0.0f, currentRadius * std::tan(sunPitch / 1.8f), 0.0f};// leve la tete
    float currentHeight = glm::mix(height, 0.5f, sunElev);//s'abaisse
    vec3 flatSunDir = glm::normalize(vec3{safeSunDir.x, 0.0f, safeSunDir.z});//pas de plonger sous le sol

    //calcul position
    //recule direction opposée au soleil ->en face.
    //on se décale sur le côté (+right) pour decentre l helico
    //les coeffs (0.8f et 0.6f) magnitude globale du rayon 
    //pythagore : 0.8^2 + 0.6^2 = 1

    vec3 offset = -flatSunDir * currentRadius;
    m_position = target + offset + vec3{0.0f, currentHeight, 0.0f};
}

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

void Camera::cut() noexcept {
    /* La poursuite recale son cap net (m_followInit) et saute à sa position : la
       sentinelle (0,0,0) déclenche le calage instantané au prochain appel de chase().
       Les vues orbite et cockpit fixent déjà la position directement (cut implicite). */
    m_followInit = false;
    m_position   = vec3{0.0f, 0.0f, 0.0f};
}

mat4 Camera::view() const noexcept {
    return glm::lookAt(m_position, m_target, m_up);
}

mat4 Camera::proj() const noexcept {
    return glm::perspective(m_fovY, m_aspect, m_near, m_far);
}

}  /* namespace artouste::render */
