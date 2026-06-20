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

void Turbine::update(float dt) noexcept {
    switch (m_state) {
        case State::Demarrage:
            /* La turbine monte seule en régime ; le rotor reste immobile, pales
             * arrêtées. */
            m_turbine += dt / TURBINE_START_TIME;
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
            if (m_brakeTimer >= ROTOR_BRAKE_DELAY) {
                m_state = State::Embrayage;  /* frein lâché : le rotor s'engage */
            }
            break;
        case State::Embrayage:
            /* Frein lâché : le rotor s'engage par la roue libre (celle qui permet
             * aussi l'autorotation) et les pales accélèrent jusqu'au régime de vol. */
            m_rotor += dt / ROTOR_ENGAGE_TIME;
            if (m_rotor >= 1.0f) {
                m_rotor = 1.0f;
                m_state = State::Regime;  /* régime établi */
            }
            break;
        case State::Extinction:
            /* Coupure : le rotor redescend (inertie), la turbine s'éteint avec lui. */
            m_rotor -= dt / TURBINE_STOP_TIME;
            if (m_rotor < 0.0f) {
                m_rotor = 0.0f;
            }
            m_turbine -= dt / TURBINE_STOP_TIME;
            if (m_turbine < 0.0f) {
                m_turbine = 0.0f;
            }
            if (m_rotor <= 0.0f && m_turbine <= 0.0f) {
                m_state = State::Arret;  /* tout est arrêté */
            }
            break;
        case State::Arret:
        case State::Regime:
            break;  /* états stables : rien à faire */
    }
}

void Turbine::forceRunning() noexcept {
    m_state   = State::Regime;
    m_turbine = 1.0f;
    m_rotor   = 1.0f;
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
