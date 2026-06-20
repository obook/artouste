/*
 * FlightModel.cpp
 * Calcul du vol : à chaque pas, on additionne les forces et les couples qui
 * s'exercent sur l'hélicoptère, on les transmet au corps rigide, puis on applique
 * les limites de sécurité et le contact avec le sol.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "physics/FlightModel.hpp"

#include "physics/constants.hpp"

#include <cmath>

namespace artouste::physics {

namespace {

/* Traînée sur un axe : -k * v * |v|. Elle s'oppose au mouvement et croît avec
 * le carré de la vitesse. */
float axisDrag(float v, float k) noexcept {
    return -k * v * std::fabs(v);
}

/* Borne une valeur dans l'intervalle [-limit, +limit]. */
float clampAbs(float v, float limit) noexcept {
    return v > limit ? limit : (v < -limit ? -limit : v);
}

}  /* namespace */

void FlightModel::update(const Controls& controls, float dt) noexcept {
    /* Turbine : on fait avancer son régime avant tout le reste, car la poussée
     * et l'anti-couple en dépendent. Rotor à l'arrêt -> rotorFraction = 0. */
    m_turbine.update(dt);
    const float rotorFraction = m_turbine.rotorFraction();

    const vec3 worldUp{0.0f, 1.0f, 0.0f};
    const vec3 bodyUpWorld = m_body.orientation * worldUp;  /* axe du rotor exprimé dans le repère monde */

    /* --- Forces (repère monde) ---------------------------------------------- */
    /* Poussée du rotor, dirigée selon l'axe vertical du fuselage. Elle est calée
     * pour équilibrer le poids au collectif COLL_HOVER, et proportionnelle au
     * régime du rotor : tant que la turbine n'est pas lancée, pas de portance. */
    const float collective  = saturate(controls.collective);
    const float baseThrust  = (MASS * G / COLL_HOVER) * collective * rotorFraction;

    /* Effet de sol : près du sol, la poussée est renforcée ; ce gain diminue avec
     * la hauteur au-dessus du relief. */
    const float agl          = m_body.position.y - m_groundHeight;  /* hauteur sol (m) */
    const float height       = agl > 0.0f ? agl : 0.0f;
    const float groundEffect = 1.0f + GE_MAX * (1.0f - clamp(height / GE_HEIGHT, 0.0f, 1.0f));

    /* Effet de translation : la portance augmente avec la vitesse horizontale.
     * Sans vent, la vitesse par rapport à l'air égale la vitesse horizontale au sol. */
    const float airspeed         = glm::length(vec2{m_body.velocity.x, m_body.velocity.z});
    const float translationalGain = 1.0f + ETL_MAX * glm::smoothstep(ETL_V_LOW, ETL_V_HIGH, airspeed);

    m_lastThrust      = baseThrust * groundEffect * translationalGain;
    const vec3 thrust = bodyUpWorld * m_lastThrust;

    const vec3 gravity{0.0f, -MASS * G, 0.0f};

    /* Traînée : calculée dans le repère de l'appareil (différente selon l'axe),
     * puis ramenée dans le repère monde. */
    const vec3 velocityBody = glm::conjugate(m_body.orientation) * m_body.velocity;
    const vec3 dragBody{axisDrag(velocityBody.x, KDRAG_FWD),
                        axisDrag(velocityBody.y, KDRAG_VERT),
                        axisDrag(velocityBody.z, KDRAG_LAT)};
    const vec3 drag = m_body.orientation * dragBody;

    const vec3 force = thrust + gravity + drag;

    /* --- Couples (repère corps) --------------------------------------------- */
    /* Rappel vers l'horizontale : un couple qui ramène l'axe du rotor vers la
     * verticale du monde, exprimé dans le repère de l'appareil. Il est nul quand
     * l'appareil est à plat et ne touche pas au cap (lacet). */
    const vec3 levelBody = glm::conjugate(m_body.orientation) * glm::cross(bodyUpWorld, worldUp);

    const vec3&     w = m_body.angularVelocity;
    vec3 torque;
    torque.x = controls.cyclicLateral * ROLL_CTRL          /* roulis (autour de X) */
               + LEVEL_GAIN * levelBody.x - DAMP_ROLL * w.x;
    /* Lacet (autour de Y). Sur l'Alouette II, le rotor tourne dans le sens horaire
     * vu de dessus : son couple de réaction fait partir le nez vers la gauche, et le
     * pilote compense au palonnier droit. D'où le signe + sur l'anti-couple, qui croît
     * avec le collectif, et le palonnier droit qui ramène le nez vers la droite. */
    torque.y = -controls.pedals * YAW_CTRL
               + REACTIVE_TORQUE * (collective - COLL_HOVER) * rotorFraction - DAMP_YAW * w.y;
    torque.z = -controls.cyclicLongitudinal * PITCH_CTRL   /* tangage (autour de Z) */
               + LEVEL_GAIN * levelBody.z - DAMP_PITCH * w.z;

    const vec3 inertia{I_ROLL, I_YAW, I_PITCH};
    m_body.integrate(force, torque, MASS, inertia, dt);

    /* --- Garde-fous numériques ------------------------------------ */
    /* On limite vitesses et rotations pour que la simulation reste stable. */
    m_body.velocity.x = clampAbs(m_body.velocity.x, MAX_SPEED);
    m_body.velocity.y = clampAbs(m_body.velocity.y, MAX_SPEED);
    m_body.velocity.z = clampAbs(m_body.velocity.z, MAX_SPEED);
    m_body.angularVelocity.x = clampAbs(m_body.angularVelocity.x, MAX_OMEGA);
    m_body.angularVelocity.y = clampAbs(m_body.angularVelocity.y, MAX_OMEGA);
    m_body.angularVelocity.z = clampAbs(m_body.angularVelocity.z, MAX_OMEGA);

    /* Contact avec le sol : l'appareil ne descend pas sous le relief. */
    if (m_body.position.y < m_groundHeight) {
        m_body.position.y = m_groundHeight;
        if (m_body.velocity.y < 0.0f) {
            m_body.velocity.y = 0.0f;
        }
    }

    /* Posé sur les patins : tant que la poussée ne dépasse pas le poids, l'appareil
     * reste collé au sol, sans glisser ni tourner. Dès que le collectif suffit à
     * le soulever, il décolle normalement. */
    if (m_body.position.y <= m_groundHeight && m_lastThrust <= MASS * G) {
        m_body.position.y      = m_groundHeight;
        m_body.velocity        = vec3{0.0f, 0.0f, 0.0f};
        m_body.angularVelocity = vec3{0.0f, 0.0f, 0.0f};
    }
}

}  /* namespace artouste::physics */
