/*
 * Camera.hpp
 * Caméra en perspective : elle calcule comment on regarde la scène.
 * Plusieurs modes sont prévus : tourner autour de l'appareil, le suivre
 * de derrière (vue poursuite), ou se placer dans le cockpit.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::render {

class Camera {
public:
    void setAspect(float aspect) noexcept { m_aspect = aspect; }

    /* Place la caméra sur un cercle par rapport au soleil pour le voir */
    void orbitSolar(const vec3& target, const vec3& sunDir, float radius, float height) noexcept;
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

    /* Tremblement de la caméra : petit décalage de l'oeil (en mètres, repère monde)
       appliqué dans l'espace vue par view(), pour faire trembler toute l'image d'un
       bloc. À remettre à zéro hors vue cockpit. Voir view() pour le pourquoi. */
    void setShake(const vec3& shake) noexcept { m_shake = shake; }

    /* Coupe franche entre deux vues : réinitialise le lissage de la poursuite pour
       qu'elle se cale instantanément (pas de glissement) au prochain appel. */
    void cut() noexcept;

    void setFovYDeg(float degrees) noexcept { m_fovY = degrees * DEG_TO_RAD; }

    /* Distance du plan proche (m). En vue cockpit, la verrière est à quelques
       centimètres de l'oeil : il faut un near petit pour ne pas la couper. En
       vues externes, un near plus grand préserve la précision de profondeur au
       loin (terrain et mer). */
    void setNear(float n) noexcept { m_near = n; }

    [[nodiscard]] mat4 view() const noexcept;
    [[nodiscard]] mat4 proj() const noexcept;
    [[nodiscard]] const vec3& position() const noexcept { return m_position; }

private:
    vec3  m_position{0.0f, 0.0f, 0.0f};
    vec3  m_target{0.0f, 0.0f, 0.0f};
    vec3  m_up{0.0f, 1.0f, 0.0f};
    vec3  m_shake{0.0f, 0.0f, 0.0f};  /* tremblement cockpit, appliqué en espace vue */
    float m_followYaw   = 0.0f;
    bool  m_followInit  = false;  /* premier appel de chase : on se cale net */
    float m_fovY        = HALF_PI / 1.5f;  /* angle de vue vertical : 60 degrés */
    float m_aspect      = 16.0f / 9.0f;
    /* near volontairement à 0.5 (pas 0.1) : avec un far très lointain, un near
       trop petit gaspille la précision du tampon de profondeur et fait
       scintiller (z-fighting) le terrain et la mer au loin. */
    float m_near        = 0.5f;
    float m_far         = 40000.0f;  /* assez loin pour voir la côte et le relief en entier */
};

}  /* namespace artouste::render */
