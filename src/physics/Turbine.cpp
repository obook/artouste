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
#include "util/Math.hpp"

#include <algorithm>
#include <cmath>

namespace artouste::physics {

namespace {

/* Battement du régime établi : deux sinus de périodes incommensurables, pour
   que l'oreille n'y entende pas une pulsation régulière. */
float battement(float t, float amplitude, float hz1, float hz2) noexcept {
    return amplitude * (0.6f * std::sin(TWO_PI * hz1 * t)
                        + 0.4f * std::sin(TWO_PI * hz2 * t + 1.7f));
}

}  /* namespace */

/* Montée en régime, en deux temps séparés par l'allumage. Avant : le démarreur
   lance seul le compresseur, vite d'abord puis de moins en moins, la résistance
   croissant avec la vitesse. Après : le carburant brûle, l'accélération repart
   franchement, puis le régulateur la modère jusqu'à une arrivée asymptotique. */
float Turbine::regimeMontee(float p) noexcept {
    const float pc = std::clamp(p, 0.0f, 1.0f);
    if (pc < ALLUMAGE_INSTANT) {
        const float u = pc / ALLUMAGE_INSTANT;
        return ALLUMAGE_REGIME * (1.0f - (1.0f - u) * (1.0f - u));
    }
    const float u = (pc - ALLUMAGE_INSTANT) / (1.0f - ALLUMAGE_INSTANT);
    return ALLUMAGE_REGIME
           + (1.0f - ALLUMAGE_REGIME) * (1.0f - std::pow(1.0f - u, MONTEE_EXPOSANT));
}

float Turbine::progresMontee(float regime) noexcept {
    const float r = std::clamp(regime, 0.0f, 1.0f);
    if (r < ALLUMAGE_REGIME) {
        const float u = 1.0f - std::sqrt(1.0f - r / ALLUMAGE_REGIME);
        return u * ALLUMAGE_INSTANT;
    }
    const float reste = 1.0f - (r - ALLUMAGE_REGIME) / (1.0f - ALLUMAGE_REGIME);
    const float u     = 1.0f - std::pow(std::max(reste, 0.0f), 1.0f / MONTEE_EXPOSANT);
    return ALLUMAGE_INSTANT + u * (1.0f - ALLUMAGE_INSTANT);
}

/* Embrayage centrifuge : couple maximal au premier contact, quand le glissement
   est grand, puis de moins en moins à mesure que les vitesses se rejoignent. */
float Turbine::regimeEmbrayage(float p) noexcept {
    const float u = std::clamp(p, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - u, EMBRAYAGE_EXPOSANT);
}

float Turbine::progresEmbrayage(float regime) noexcept {
    const float r = std::clamp(regime, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - r, 1.0f / EMBRAYAGE_EXPOSANT);
}

/* Extinction : plus de carburant, il ne reste que la traînée et les
   frottements. Chute franche, puis longue traîne dans les bas régimes. */
float Turbine::regimeExtinction(float p) noexcept {
    const float u = std::clamp(p, 0.0f, 1.0f);
    return std::pow(1.0f - u, EXTINCTION_EXPOSANT);
}

float Turbine::progresExtinction(float regime) noexcept {
    const float r = std::clamp(regime, 0.0f, 1.0f);
    return 1.0f - std::pow(r, 1.0f / EXTINCTION_EXPOSANT);
}

void Turbine::toggle() noexcept {
    switch (m_state) {
        case State::Arret:
        case State::Extinction:
            /* (Re)lance la turbine, en reprenant la montée là où le régime
               courant la situe : une turbine coupée puis relancée aussitôt
               repart de son régime, pas de zéro. */
            m_progresTurbine = progresMontee(m_turbine);
            m_state          = State::Demarrage;
            break;
        case State::Demarrage:
        case State::Attente:
        case State::Embrayage:
        case State::Regime:
            m_progresTurbine = progresExtinction(m_turbine);
            m_progresRotor   = progresExtinction(m_rotor);
            m_state          = State::Extinction;  /* coupe la turbine */
            break;
    }
}

void Turbine::update(float dt, float t4CibleC, float pasDeg) noexcept {
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
            m_progresTurbine += dt / turbineStartTime;
            m_turbine = regimeMontee(m_progresTurbine);
            if (m_progresTurbine >= 1.0f) {
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
                m_progresRotor = progresEmbrayage(m_rotor);
                m_state        = State::Embrayage;  /* frein lâché : le rotor s'engage */
            }
            break;
        case State::Embrayage:
            /* Frein lâché : le rotor s'engage par la roue libre (celle qui permet
             * aussi l'autorotation) et les pales accélèrent jusqu'au régime de vol. */
            m_progresRotor += dt / rotorEngageTime;
            m_rotor = regimeEmbrayage(m_progresRotor);
            if (m_progresRotor >= 1.0f) {
                m_rotor     = 1.0f;
                m_fastStart = false;  /* régime atteint : le démarrage rapide est terminé */
                m_state     = State::Regime;  /* régime établi */
            }
            break;
        case State::Extinction:
            /* Coupure : la turbine s'éteint, et le rotor redescend plus lentement
             * encore, porté par sa forte inertie (roue libre). */
            m_progresRotor += dt / ROTOR_STOP_TIME;
            m_rotor = regimeExtinction(m_progresRotor);
            if (m_progresRotor >= 1.0f) {
                m_rotor = 0.0f;
            }
            m_progresTurbine += dt / TURBINE_STOP_TIME;
            m_turbine = regimeExtinction(m_progresTurbine);
            if (m_progresTurbine >= 1.0f) {
                m_turbine = 0.0f;
            }
            m_regimeS = 0.0f;
            m_fastStart = false;  /* une coupure annule le démarrage rapide en cours */
            if (m_rotor <= 0.0f && m_turbine <= 0.0f) {
                m_state = State::Arret;  /* tout est arrêté */
            }
            break;
        case State::Arret:
        case State::Regime:
            break;  /* états stables : rien à faire */
    }

    /* Régime établi : la turbine respire autour du nominal, et fléchit sous la
       charge. Hors régime établi, le régulateur suit le pas sans retard, pour
       qu'un collectif déjà tiré au moment de l'embrayage ne produise pas un faux
       creux à la seconde même où le rotor s'accouple. */
    if (m_state != State::Regime) {
        m_pasLisse = pasDeg;
    }
    if (m_state == State::Attente || m_state == State::Embrayage
        || m_state == State::Regime) {
        m_regimeS += dt;
        float facteur = 1.0f
                        + battement(m_regimeS, BATTEMENT_REGIME, BATTEMENT_REGIME_HZ1,
                                    BATTEMENT_REGIME_HZ2);
        if (m_state == State::Regime) {
            m_pasLisse = lowPass(m_pasLisse, pasDeg, dt, DROOP_TAU_S);
            /* Statisme : ne joue qu'au-dessus du pas de sustentation. */
            const float charge = std::max(0.0f, pasDeg - DROOP_PAS_REF_DEG)
                                 / (PAS_MAX_DEG - DROOP_PAS_REF_DEG);
            /* Creux transitoire : ce que le régulateur n'a pas encore rattrapé.
               Seulement à la montée du pas -- relâcher le collectif décharge la
               turbine, ça ne creuse pas le régime. */
            const float retard = std::max(0.0f, pasDeg - m_pasLisse)
                                 / (PAS_MAX_DEG - PAS_MIN_DEG);
            facteur -= DROOP_STATIQUE * charge + DROOP_TRANSITOIRE * retard;
        }
        m_turbine = facteur;
        /* Monoarbre : une fois l'embrayage fermé, le rotor suit la turbine dans
           le rapport fixe du réducteur. Battement et droop sont donc les mêmes
           pour les deux. Pendant l'embrayage il glisse encore, et sa progression
           se lit dans m_rotor : on n'y touche pas. */
        if (m_state == State::Regime) {
            m_rotor = facteur;
        }
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
    m_progresTurbine = 1.0f;
    m_progresRotor   = 1.0f;
    m_regimeS   = 0.0f;
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
    m_progresTurbine = 0.0f;
    m_progresRotor   = 0.0f;
    m_regimeS        = 0.0f;
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
