/*
 * Camera.hpp
 * Caméra en perspective : elle calcule comment on regarde la scène.
 * Plusieurs modes sont prévus : tourner autour de l'appareil, le suivre
 * de derrière (vue poursuite), ou se placer dans le cockpit.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::render {

class Camera {
public:
    void setAspect(float aspect) noexcept { m_aspect = aspect; }

    /* Place la caméra sur un cercle horizontal autour de target. */
    void orbit(const vec3& target, float radius, float height, float angleRad) noexcept;

    /*
     * Vue poursuite : derrière et au-dessus de l'appareil, suit sa
     * position et son cap avec un léger retard pour rester fluide.
     * dt est la durée de la dernière image, utilisée pour le lissage.
     */
    void chase(const vec3& target, float yaw, float dt) noexcept;

    /* Vue imposée (cockpit) : fixée à l'appareil, sans lissage. */
    void setLookAt(const vec3& eye, const vec3& target, const vec3& up) noexcept;

    void setFovYDeg(float degrees) noexcept { m_fovY = degrees * DEG_TO_RAD; }

    [[nodiscard]] mat4 view() const noexcept;
    [[nodiscard]] mat4 proj() const noexcept;
    [[nodiscard]] const vec3& position() const noexcept { return m_position; }

private:
    vec3  m_position{0.0f, 0.0f, 0.0f};
    vec3  m_target{0.0f, 0.0f, 0.0f};
    vec3  m_up{0.0f, 1.0f, 0.0f};
    float m_followYaw   = 0.0f;
    bool  m_followInit  = false;  /* premier appel de chase : on se cale net */
    float m_fovY        = HALF_PI / 1.5f;  /* angle de vue vertical : 60 degrés */
    float m_aspect      = 16.0f / 9.0f;
    float m_near        = 0.1f;
    float m_far         = 5000.0f;
};

}  /* namespace artouste::render */
