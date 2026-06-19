/*
 * ArcadeFlight.cpp
 * Mise à jour du vol arcade : les commandes sont converties en vitesses, qui
 * font ensuite avancer l'appareil. Tout est lissé pour un rendu agréable.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "physics/ArcadeFlight.hpp"

#include <cmath>

namespace artouste::physics {

namespace {

/* Réglages du pilotage arcade. Ces valeurs visent une prise en main douce,
 * pas le réalisme (qui viendra avec le vrai modèle de vol). */
constexpr float YAW_RATE     = 1.20f;   /* rad/s à fond de palonnier (~69 deg/s) */
constexpr float MAX_SPEED    = 25.0f;   /* m/s de déplacement à fond de cyclique */
constexpr float MAX_CLIMB    = 6.0f;    /* m/s de montée à collectif plein */
constexpr float MAX_PITCH    = 0.35f;   /* rad d'assiette en tangage (~20 deg) */
constexpr float MAX_ROLL     = 0.45f;   /* rad d'assiette en roulis  (~26 deg) */
constexpr float VEL_TAU      = 0.40f;   /* lissage des vitesses de déplacement (s) */
constexpr float ATT_TAU      = 0.15f;   /* lissage de l'assiette (s) */

/* Rapproche progressivement "current" de "target" : la valeur se rapproche d'autant
 * plus vite que tau est petit. C'est un filtre passe-bas du premier ordre. */
float approach(float current, float target, float dt, float tau) noexcept {
    if (tau <= 0.0f) {
        return target;
    }
    const float alpha = 1.0f - std::exp(-dt / tau);
    return current + alpha * (target - current);
}

}  /* namespace */

void ArcadeFlight::update(const Controls& controls, float dt) noexcept {
    /* Lacet : on ajoute directement la rotation commandée à l'orientation. */
    m_state.yaw += controls.pedals * YAW_RATE * dt;

    /* Assiette : pur effet visuel qui rejoint doucement la cible. Le nez pique
     * (tangage négatif) quand on commande l'avancée. */
    const float targetPitch = -controls.cyclicLongitudinal * MAX_PITCH;
    const float targetRoll  = controls.cyclicLateral * MAX_ROLL;
    m_state.pitch = approach(m_state.pitch, targetPitch, dt, ATT_TAU);
    m_state.roll  = approach(m_state.roll, targetRoll, dt, ATT_TAU);

    /* Vitesse voulue dans le repère de l'appareil : avant (+X) et droite (+Z).
     * On la fait ensuite tourner selon le lacet pour l'exprimer dans le repère monde. */
    const float vForward = controls.cyclicLongitudinal * MAX_SPEED;
    const float vRight   = controls.cyclicLateral * MAX_SPEED;
    const float c        = std::cos(m_state.yaw);
    const float s        = std::sin(m_state.yaw);

    vec3 targetVel;
    targetVel.x = vForward * c + vRight * s;
    targetVel.z = -vForward * s + vRight * c;
    /* Collectif au milieu (0,5) = vol stationnaire ; au-dessus on monte, en dessous on descend. */
    targetVel.y = (controls.collective - 0.5f) * 2.0f * MAX_CLIMB;

    m_state.velocity.x = approach(m_state.velocity.x, targetVel.x, dt, VEL_TAU);
    m_state.velocity.z = approach(m_state.velocity.z, targetVel.z, dt, VEL_TAU);
    m_state.velocity.y = approach(m_state.velocity.y, targetVel.y, dt, VEL_TAU);

    m_state.position += m_state.velocity * dt;

    /* Garde-fou : l'appareil ne descend pas sous le sol. */
    if (m_state.position.y < 0.0f) {
        m_state.position.y = 0.0f;
        if (m_state.velocity.y < 0.0f) {
            m_state.velocity.y = 0.0f;
        }
    }
}

}  /* namespace artouste::physics */
