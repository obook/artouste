// Caméra perspective. Pour M1 elle orbite autour de l'hélicoptère statique ;
// la vue chase amortie et la vue cockpit viendront plus tard.

#pragma once

#include "util/Math.hpp"

namespace artouste::render {

class Camera {
public:
    void setAspect(float aspect) noexcept { m_aspect = aspect; }

    // Place la caméra sur un cercle horizontal autour de target.
    void orbit(const vec3& target, float radius, float height, float angleRad) noexcept;

    // Vue chase : derrière et au-dessus de l'appareil, suivant
    // sa position et son cap avec un léger amortissement. dt sert au lissage.
    void chase(const vec3& target, float yaw, float dt) noexcept;

    // Vue explicite (cockpit) : solidaire de l'appareil, pas de lissage.
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
    bool  m_followInit  = false;  // premier appel de chase : on se cale net
    float m_fovY        = HALF_PI / 1.5f;  // 60 degrés
    float m_aspect      = 16.0f / 9.0f;
    float m_near        = 0.1f;
    float m_far         = 5000.0f;
};

}  // namespace artouste::render
