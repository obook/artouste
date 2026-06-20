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
            /* La turbine monte seule en régime ; le rotor reste immobile, pales
             * arrêtées. Dans la réalité, le pilote a libéré le frein rotor au
             * préalable (geste non modélisé ici). */
            m_turbine += dt / TURBINE_START_TIME;
            if (m_turbine >= 1.0f) {
                m_turbine = 1.0f;
                m_state   = State::Embrayage;  /* seuil atteint : le rotor s'engage */
            }
            break;
        case State::Embrayage:
            /* Au-delà du seuil de régime turbine, le rotor s'engage automatiquement
             * par la roue libre (celle qui permet aussi l'autorotation) et les pales
             * accélèrent jusqu'au régime de vol. */
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
