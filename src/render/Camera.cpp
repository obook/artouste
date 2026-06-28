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

    /* Azimut : la caméra se place à l'opposé du soleil (soleil dans le dos) pour
       éclairer l'appareil de face, plan "golden hour". On prend la direction du
       soleil projetée au sol ; si le soleil est trop haut (composantes x, z quasi
       nulles), on retombe sur un azimut fixe pour rester stable. */
    vec3        flat        = vec3{sunDir.x, 0.0f, sunDir.z};
    const float flatLen     = glm::length(flat);
    const vec3  flatSunDir  = (flatLen > 1e-3f) ? flat / flatLen : vec3{0.0f, 0.0f, 1.0f};

    /* Hauteur de caméra : soleil bas -> caméra basse (lumière rasante) ; quand le
       soleil remonte, elle reprend sa hauteur normale. sunElev borné à [0, 1]. */
    const float sunElev       = glm::clamp(sunDir.y, 0.0f, 1.0f);
    const float currentHeight = glm::mix(height * 0.45f, height, sunElev);

    /* Visée légèrement au-dessus du centre de l'appareil : il se pose dans le bas
       du cadre, le ciel et le soleil occupent le haut. La montée est bornée pour
       ne JAMAIS pousser l'appareil hors champ (c'était le défaut précédent : à
       midi la cible montait de plusieurs dizaines de mètres). */
    const float targetLift = glm::clamp(radius * 0.08f, 0.0f, 1.5f);
    m_target               = target + vec3{0.0f, targetLift, 0.0f};

    m_position = target - flatSunDir * radius + vec3{0.0f, currentHeight, 0.0f};
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
    mat4 v = glm::lookAt(m_position, m_target, m_up);

    /* Tremblement du cockpit. On déplace l'oeil d'un petit vecteur monde m_shake,
       mais SANS l'ajouter à la position monde : à des milliers de mètres (terrain
       réel), ce décalage de quelques millimètres serait noyé par l'arrondi flottant
       et seul l'appareil, dessiné en coordonnées proches, tremblerait (le paysage,
       en grandes coordonnées, restait immobile). On l'applique donc dans l'espace
       vue, en petits nombres : un oeil déplacé de w décale toute la scène de -R*w
       dans la vue, ce qui fait trembler le paysage exactement comme le reste. */
    const vec3 shiftEye = mat3(v) * m_shake;
    v[3].x -= shiftEye.x;
    v[3].y -= shiftEye.y;
    v[3].z -= shiftEye.z;
    return v;
}

mat4 Camera::proj() const noexcept {
    return glm::perspective(m_fovY, m_aspect, m_near, m_far);
}

}  /* namespace artouste::render */
