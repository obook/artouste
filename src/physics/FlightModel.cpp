#include "physics/FlightModel.hpp"

#include "physics/constants.hpp"

#include <cmath>

namespace artouste::physics {

namespace {

// Traînée quadratique d'un axe : -k * v * |v|.
float axisDrag(float v, float k) noexcept {
    return -k * v * std::fabs(v);
}

float clampAbs(float v, float limit) noexcept {
    return v > limit ? limit : (v < -limit ? -limit : v);
}

}  // namespace

void FlightModel::update(const Controls& controls, float dt) noexcept {
    const vec3 worldUp{0.0f, 1.0f, 0.0f};
    const vec3 bodyUpWorld = m_body.orientation * worldUp;  // axe rotor en repère monde

    // --- Forces (repère monde) ----------------------------------------------
    // Poussée : le long de l'axe vertical du fuselage, calibrée pour sustenter
    // à COLL_HOVER hors effet de sol. (Le régime rotor est supposé constant.)
    const float collective  = saturate(controls.collective);
    const float baseThrust  = (MASS * G / COLL_HOVER) * collective;

    // Effet de sol : gain de poussée près du sol, décroissant avec la hauteur.
    const float height       = m_body.position.y > 0.0f ? m_body.position.y : 0.0f;
    const float groundEffect = 1.0f + GE_MAX * (1.0f - clamp(height / GE_HEIGHT, 0.0f, 1.0f));

    // Effet de translation : gain de portance avec la vitesse air horizontale
    // (sans vent, la vitesse air = vitesse sol horizontale).
    const float airspeed         = glm::length(vec2{m_body.velocity.x, m_body.velocity.z});
    const float translationalGain = 1.0f + ETL_MAX * glm::smoothstep(ETL_V_LOW, ETL_V_HIGH, airspeed);

    m_lastThrust      = baseThrust * groundEffect * translationalGain;
    const vec3 thrust = bodyUpWorld * m_lastThrust;

    const vec3 gravity{0.0f, -MASS * G, 0.0f};

    // Traînée : calculée en repère corps (anisotrope) puis ramenée au monde.
    const vec3 velocityBody = glm::conjugate(m_body.orientation) * m_body.velocity;
    const vec3 dragBody{axisDrag(velocityBody.x, KDRAG_FWD),
                        axisDrag(velocityBody.y, KDRAG_VERT),
                        axisDrag(velocityBody.z, KDRAG_LAT)};
    const vec3 drag = m_body.orientation * dragBody;

    const vec3 force = thrust + gravity + drag;

    // --- Moments (repère corps) ---------------------------------------------
    // Rappel vers l'horizontale : axe de rotation ramenant l'axe rotor vers la
    // verticale monde, exprimé en repère corps. Nul quand l'appareil est à plat,
    // sans composante de lacet (le cap n'est pas contraint).
    const vec3 levelBody = glm::conjugate(m_body.orientation) * glm::cross(bodyUpWorld, worldUp);

    const vec3&     w = m_body.angularVelocity;
    vec3 torque;
    torque.x = controls.cyclicLateral * ROLL_CTRL          // roulis (autour de X)
               + LEVEL_GAIN * levelBody.x - DAMP_ROLL * w.x;
    torque.y = controls.pedals * YAW_CTRL                  // lacet (autour de Y)
               - REACTIVE_TORQUE * (collective - COLL_HOVER) - DAMP_YAW * w.y;
    torque.z = -controls.cyclicLongitudinal * PITCH_CTRL   // tangage (autour de Z)
               + LEVEL_GAIN * levelBody.z - DAMP_PITCH * w.z;

    const vec3 inertia{I_ROLL, I_YAW, I_PITCH};
    m_body.integrate(force, torque, MASS, inertia, dt);

    // --- Garde-fous numériques ------------------------------------
    m_body.velocity.x = clampAbs(m_body.velocity.x, MAX_SPEED);
    m_body.velocity.y = clampAbs(m_body.velocity.y, MAX_SPEED);
    m_body.velocity.z = clampAbs(m_body.velocity.z, MAX_SPEED);
    m_body.angularVelocity.x = clampAbs(m_body.angularVelocity.x, MAX_OMEGA);
    m_body.angularVelocity.y = clampAbs(m_body.angularVelocity.y, MAX_OMEGA);
    m_body.angularVelocity.z = clampAbs(m_body.angularVelocity.z, MAX_OMEGA);

    // Contact sol : on ne descend pas sous le terrain.
    if (m_body.position.y < 0.0f) {
        m_body.position.y = 0.0f;
        if (m_body.velocity.y < 0.0f) {
            m_body.velocity.y = 0.0f;
        }
    }
}

}  // namespace artouste::physics
