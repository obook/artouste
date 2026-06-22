/*
 * Turbine.cpp
 * Logique de la turbine : transitions d'état et montée/descente des régimes
 * turbine puis rotor.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "physics/Turbine.hpp"

#include "physics/constants.hpp"

#include <cmath>

namespace artouste::physics {

void Turbine::toggle() noexcept {
    switch (m_state) {
        case State::Arret:
        case State::Extinction:
            m_state = State::Demarrage;  /* (re)lance la turbine */
            break;
        case State::Demarrage:
        case State::Attente:
        case State::Embrayage:
        case State::Regime:
            m_state = State::Extinction;  /* coupe la turbine */
            break;
    }
}

void Turbine::update(float dt, float collective) noexcept {
    /* Durées de la montée en régime : raccourcies pendant un démarrage rapide
     * (mode démo), sinon celles de la séquence réelle. */
    const float turbineStartTime = m_fastStart ? DEMO_TURBINE_START_TIME : TURBINE_START_TIME;
    const float rotorBrakeDelay  = m_fastStart ? DEMO_ROTOR_BRAKE_DELAY  : ROTOR_BRAKE_DELAY;
    const float rotorEngageTime  = m_fastStart ? DEMO_ROTOR_ENGAGE_TIME  : ROTOR_ENGAGE_TIME;

    switch (m_state) {
        case State::Demarrage:
            /* La turbine monte seule en régime ; le rotor reste immobile, pales
             * arrêtées. */
            m_turbine += dt / turbineStartTime;
            if (m_turbine >= 1.0f) {
                m_turbine    = 1.0f;
                m_brakeTimer = 0.0f;
                m_state      = State::Attente;  /* turbine au régime, frein encore serré */
            }
            break;
        case State::Attente:
            /* La turbine tient son plein régime, mais le frein rotor est encore
             * serré : les pales restent immobiles le temps que le pilote le lâche.
             * Ce délai écoulé, on passe à l'embrayage du rotor. */
            m_brakeTimer += dt;
            if (m_brakeTimer >= rotorBrakeDelay) {
                m_state = State::Embrayage;  /* frein lâché : le rotor s'engage */
            }
            break;
        case State::Embrayage:
            /* Frein lâché : le rotor s'engage par la roue libre (celle qui permet
             * aussi l'autorotation) et les pales accélèrent jusqu'au régime de vol. */
            m_rotor += dt / rotorEngageTime;
            if (m_rotor >= 1.0f) {
                m_rotor     = 1.0f;
                m_fastStart = false;  /* régime atteint : le démarrage rapide est terminé */
                m_state     = State::Regime;  /* régime établi */
            }
            break;
        case State::Extinction:
            /* Coupure : la turbine s'éteint, et le rotor redescend plus lentement
             * encore, porté par sa forte inertie (roue libre). */
            m_rotor -= dt / ROTOR_STOP_TIME;
            if (m_rotor < 0.0f) {
                m_rotor = 0.0f;
            }
            m_turbine -= dt / TURBINE_STOP_TIME;
            if (m_turbine < 0.0f) {
                m_turbine = 0.0f;
            }
            m_fastStart = false;  /* une coupure annule le démarrage rapide en cours */
            if (m_rotor <= 0.0f && m_turbine <= 0.0f) {
                m_state = State::Arret;  /* tout est arrêté */
            }
            break;
        case State::Arret:
        case State::Regime:
            break;  /* états stables : rien à faire */
    }

    /* Température de la tuyère : cible déduite du régime turbine (la tuyère chauffe
     * en montant en régime) et de la charge collective, rejointe avec une inertie
     * thermique. La charge intervient au carré : la tuyère reste fraîche en
     * croisière et ne chauffe vraiment qu'à fort collectif, si bien que l'alerte
     * (480 degrés) n'apparaît qu'au-delà des trois quarts du collectif. */
    const float load   = collective < 0.0f ? 0.0f : (collective > 1.0f ? 1.0f : collective);
    const float target = EXHAUST_TEMP_AMBIENT_C
                       + (EXHAUST_TEMP_IDLE_C - EXHAUST_TEMP_AMBIENT_C) * m_turbine
                       + (EXHAUST_TEMP_MAX_C - EXHAUST_TEMP_IDLE_C) * m_turbine * load * load;
    const float ease   = 1.0f - std::exp(-dt / EXHAUST_TEMP_TAU);
    m_exhaustC += (target - m_exhaustC) * ease;
}

void Turbine::forceRunning() noexcept {
    m_state     = State::Regime;
    m_turbine   = 1.0f;
    m_rotor     = 1.0f;
    m_fastStart = false;
    m_exhaustC  = EXHAUST_TEMP_IDLE_C;  /* tuyère déjà chaude (régime établi, charge minimale) */
}

void Turbine::startFast() noexcept {
    if (m_state == State::Arret || m_state == State::Extinction) {
        m_state     = State::Demarrage;  /* relance la turbine, durées DEMO_* */
        m_fastStart = true;
    }
}

void Turbine::stopNow() noexcept {
    m_state      = State::Arret;
    m_turbine    = 0.0f;
    m_rotor      = 0.0f;
    m_fastStart  = false;
    m_brakeTimer = 0.0f;
    m_exhaustC   = EXHAUST_TEMP_AMBIENT_C;
}

const char* Turbine::label() const noexcept {
    switch (m_state) {
        case State::Arret:      return "ARRÊT";
        case State::Demarrage:  return "DÉMARRAGE";
        case State::Attente:    return "FREIN ROTOR";
        case State::Embrayage:  return "EMBRAYAGE";
        case State::Regime:     return "EN RÉGIME";
        case State::Extinction: return "EXTINCTION";
    }
    return "ARRÊT";
}

}  /* namespace artouste::physics */
