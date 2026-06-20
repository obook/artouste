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
        case State::Embrayage:
        case State::Regime:
            m_state = State::Extinction;  /* coupe la turbine */
            break;
    }
}

void Turbine::update(float dt) noexcept {
    switch (m_state) {
        case State::Demarrage:
            /* La turbine monte seule en régime ; le rotor reste retenu par le frein,
             * pales immobiles. */
            m_turbine += dt / TURBINE_START_TIME;
            if (m_turbine >= 1.0f) {
                m_turbine = 1.0f;
                m_state   = State::Embrayage;  /* turbine lancée, frein relâché */
            }
            break;
        case State::Embrayage:
            /* Frein relâché : la turbine libre entraîne le rotor par la roue libre,
             * les pales s'accélèrent. */
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
        case State::Embrayage:  return "EMBRAYAGE";
        case State::Regime:     return "EN RÉGIME";
        case State::Extinction: return "EXTINCTION";
    }
    return "ARRÊT";
}

}  /* namespace artouste::physics */
