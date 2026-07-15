/*
 * ApplicationRotorRadio.cpp
 * Animation du rotor principal et message radio de la tour au décollage : deux
 * étapes par image appelées depuis mainLoop() (voir ApplicationLoop.cpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "app/AppConstants.hpp"
#include "physics/RigidBody.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app {

namespace {

/*
 * Rotor principal (animation visuelle) :
 *   - ROTOR_SPIN_RATE : vitesse de rotation à plein régime, en rad/s. Volontairement
 *     ralentie par rapport à la réalité pour éviter l'effet stroboscopique.
 *   - BLADE_SPACING : écart entre deux pales (rotor tripale -> 120 degrés). Les
 *     positions de parking sont les multiples de cet écart : une pale alignée sur
 *     l'axe de l'appareil, les deux autres encadrant la sortie d'échappement.
 *   - PARK_TAU : constante de temps du filet de sécurité qui résorbe, à l'arrêt,
 *     le résidu d'angle laissé par le guidage de l'extinction (quasi nul).
 *   - PARK_STEER_MIN/MAX : bornes du facteur de guidage pendant l'extinction
 *     (correction de quelques % au plus, invisible à l'oeil).
 */
constexpr float ROTOR_SPIN_RATE = 16.0f;
constexpr float BLADE_SPACING   = 2.0944f;  /* 2*pi/3 rad ~ 120 degrés */
constexpr float PARK_TAU        = 0.6f;
constexpr float PARK_STEER_MIN  = 0.90f;
constexpr float PARK_STEER_MAX  = 1.10f;

}  /* namespace */

void Application::advanceRotor(float rotorFraction, float frameDt) {
    /* Le rotor n'avance qu'au prorata du régime (donc pales immobiles turbine
     * coupée, puis accélération), dans le sens horaire vu de dessus (angle
     * décroissant). La position de parking visée est un multiple de 120° décalé
     * du jitter m_parkOffset : une pale presque dans l'axe de l'appareil, les
     * deux autres de part et d'autre de la sortie de la turbine, pour qu'aucune
     * ne stationne dans le jet chaud. Figé en pause, comme le reste de la
     * simulation. */
    if (m_paused) {
        return;
    }
    if (rotorFraction > 0.0f) {
        float advance = rotorFraction * ROTOR_SPIN_RATE * frameDt;
        /* Extinction : le régime rotor descend linéairement (ROTOR_STOP_TIME),
           donc l'angle où il s'immobilisera se prédit : il reste à balayer
           RATE x f^2 x T/2. On corrige alors imperceptiblement la rotation
           (quelques pour mille en pratique, borné à +/-10 %) pour que le rotor
           meure PILE sur une position de parking, comme freiné par son propre
           frottement : aucun recalage après l'arrêt. Recalculée à chaque image,
           la correction résorbe d'elle-même les erreurs d'intégration. */
        if (m_flight.turbine().state() == physics::Turbine::State::Extinction) {
            const float sweep = ROTOR_SPIN_RATE * rotorFraction * rotorFraction
                                * physics::ROTOR_STOP_TIME * 0.5f;
            if (sweep > 1e-4f) {
                const float stopAt = m_rotorAngle - sweep;
                const float notch =
                    std::round((stopAt - m_parkOffset) / BLADE_SPACING) * BLADE_SPACING
                    + m_parkOffset;
                advance *= clamp(1.0f + (stopAt - notch) / sweep,
                                 PARK_STEER_MIN, PARK_STEER_MAX);
            }
        }
        m_rotorAngle -= advance;
    } else {
        /* Filet de sécurité à l'arrêt complet : résorbe en douceur le résidu
         * laissé par le guidage (quasi nul), ou recale le rotor si l'extinction
         * a été trop brève pour le guider (coupure à très bas régime). */
        const float park =
            std::round((m_rotorAngle - m_parkOffset) / BLADE_SPACING) * BLADE_SPACING
            + m_parkOffset;
        const float ease = 1.0f - std::exp(-frameDt / PARK_TAU);
        m_rotorAngle += (park - m_rotorAngle) * ease;
    }
}

void Application::updateRadioMessage(float turbineFraction, float frameDt) {
    /* Turbine nettement ralentie : on réarme pour le prochain démarrage. */
    if (turbineFraction < 0.5f) {
        m_radioMsgArmed = false;
        m_radioMsgDone  = false;
    }
    /* Turbine au plein régime : on arme un compte à rebours de 2 s (une seule fois). */
    if (turbineFraction >= 0.99f && !m_radioMsgArmed && !m_radioMsgDone) {
        m_radioMsgArmed = true;
        m_radioMsgDelay = 2.0f;
    }
    /* Délai écoulé : on émet le message (voix de synthèse) et son sous-titre. */
    if (m_radioMsgArmed) {
        m_radioMsgDelay -= frameDt;
        if (m_radioMsgDelay <= 0.0f) {
            /* La tour de contrôle de l'hélipad de départ autorise le décollage. Le nom
               de la station vient du terrain (helipads.txt), donc correct sur toute
               carte ; on retire un préfixe "Aérodrome de/d'" pour une tournure naturelle. */
            std::string station = m_homeStation;
            for (const char* prefix : {"Aérodrome de ", "Aérodrome d'"}) {
                const std::string p = prefix;
                if (station.rfind(p, 0) == 0) {
                    station = station.substr(p.size());
                    break;
                }
            }
            m_radioMsg = station.empty()
                             ? "Fox-Bravo, tower, wind calm, cleared for take-off."
                             : "Fox-Bravo, " + station + " tower, wind calm, cleared for take-off.";
            m_radioMsgShow = 7.0f;
            m_audio.playRadioMessage(m_radioMsg);
            m_radioMsgArmed = false;
            m_radioMsgDone  = true;
        }
    }
    if (m_radioMsgShow > 0.0f) {
        m_radioMsgShow -= frameDt;
    }

    /* Le rotor attend l'autorisation de la tour : on bloque son engagement tant que la
       turbine est au régime et que le message n'est pas terminé. Avant l'émission, le
       verrou est posé dès le plein régime ; après, il tient jusqu'à la fin de la voix. */
    const bool holdRotor = m_radioMsgDone ? m_audio.radioMessagePlaying()
                                          : (turbineFraction >= 0.99f);
    m_flight.turbine().setRotorHold(holdRotor);
}

}  /* namespace artouste::app */
