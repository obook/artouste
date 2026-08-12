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

#include <algorithm>
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

void Turbine::update(float dt, float t4CibleC) noexcept {
    /* Durée de la montée en régime turbine : raccourcie pendant un démarrage rapide
     * (mode démo), sinon celle de la séquence réelle. La phase rotor (frein puis
     * embrayage centrifuge) garde ses durées réelles dans tous les cas. */
    const float turbineStartTime = m_fastStart ? DEMO_TURBINE_START_TIME : TURBINE_START_TIME;
    const float rotorBrakeDelay  = ROTOR_BRAKE_DELAY;
    const float rotorEngageTime  = ROTOR_ENGAGE_TIME;

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
            if (m_brakeTimer >= rotorBrakeDelay && !m_rotorHold) {
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

    /* Température de la tuyère : l'appelant fournit la cible de plein régime
     * (loi pas/température du manuel, régime transitoire compris) ; pendant la
     * montée en régime, la cible est proportionnelle au régime turbine, et la
     * tuyère rejoint le tout avec son inertie thermique.
     *
     * Turbine coupée, la sonde ne vise PAS l'air ambiant mais la chaleur
     * résiduelle du métal qui l'entoure (voir EXHAUST_RESIDU) : elle décroche
     * donc en quelques secondes, puis suit ce métal qui met des minutes à se
     * vider. Sans ces deux temps, la TMP tombait à l'ambiante en une demi-minute,
     * ce qu'aucune turbine ne fait. */
    /* Flamme éteinte : les gaz cessent tout de suite, bien avant que la turbine
       ait fini de ralentir (TURBINE_STOP_TIME, 30 s). Lier la température des gaz
       au seul régime ferait descendre la sonde en pente douce pendant une
       demi-minute, alors qu'elle décroche en quelques secondes. */
    const bool  allumee = (m_state != State::Arret && m_state != State::Extinction);
    const float gaz = allumee
        ? EXHAUST_TEMP_AMBIENT_C + (t4CibleC - EXHAUST_TEMP_AMBIENT_C) * m_turbine
        : EXHAUST_TEMP_AMBIENT_C;
    const float chauffe = EXHAUST_TEMP_AMBIENT_C
                        + (gaz - EXHAUST_TEMP_AMBIENT_C) * EXHAUST_RESIDU;
    if (chauffe >= m_residuC) {
        m_residuC = chauffe;  /* les gaz réchauffent le métal, sans retard notable */
    } else {
        m_residuC += (EXHAUST_TEMP_AMBIENT_C - m_residuC) *
                     (1.0f - std::exp(-dt / EXHAUST_COOL_TAU));
    }
    /* La sonde voit le plus chaud des deux : les gaz tant que la turbine tourne,
       le métal ensuite. Aucun test d'état à écrire, et un redémarrage en cours de
       refroidissement repart naturellement de la température courante au lieu de
       replonger vers l'ambiante le temps que le régime remonte. */
    const float target = std::max(gaz, m_residuC);
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
    m_rotorHold  = false;
    m_exhaustC   = EXHAUST_TEMP_AMBIENT_C;
    m_residuC    = EXHAUST_TEMP_AMBIENT_C;  /* remise à froid complète : stopNow est un
                                               reset d'état, pas une extinction */
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
