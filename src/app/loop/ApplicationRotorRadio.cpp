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
#include <cstddef>
#include <iterator>
#include <random>
#include <string>

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

/*
 * Clairances de décollage de la tour.
 *
 * Le texte est synthétisé à la volée (Flite, voir AudioEngineRadio.cpp) plutôt
 * que lu dans un fichier son : le nom du terrain vient de helipads.txt, donc la
 * tour s'annonce juste sur les neuf cartes, y compris sur celles à venir. On
 * fait varier la formule pour que deux décollages ne se ressemblent pas, sans
 * jamais annoncer ce que le simulateur ne modélise pas : ni vent établi (le
 * modèle de vol l'ignore), ni trafic en approche (le ciel est vide), ni consigne
 * de cap (le pilote part où il veut). Le vent calme, lui, est exact.
 *
 * 'salutation' insère la formule d'usage calée sur l'heure simulée ; 'corps'
 * ferme le message, après le nom de la tour.
 */
struct ClairanceRadio {
    bool        salutation;
    const char* corps;
};

constexpr ClairanceRadio CLAIRANCES[] = {
    {false, "wind calm, cleared for take-off."},
    {true, "wind calm, cleared for take-off."},
    {false, "surface wind calm, pad clear, cleared for take-off."},
    {true, "wind calm, QNH one zero one three, cleared for take-off."},
    {false, "cleared for take-off, wind calm, caution downwash on the pad."},
    {true, "surface wind calm, cleared for take-off at your discretion."},
};

/* Salutation d'usage, calée sur l'heure du cycle jour/nuit : la tour ne souhaite
   pas le bonjour à deux heures du matin. */
const char* salutationRadio(float heureSecondes) {
    const float heure = heureSecondes / 3600.0f;
    if (heure >= 5.0f && heure < 12.0f) {
        return "good morning";
    }
    if (heure >= 12.0f && heure < 18.0f) {
        return "good afternoon";
    }
    return "good evening";
}

/* Tire une clairance, en écartant celle du décollage précédent : répétée deux
   fois d'affilée, la même formulation s'entend immédiatement. L'état tient dans
   des variables statiques car le tirage n'a qu'un appelant, dans la boucle
   principale, donc sur un seul fil. */
const ClairanceRadio& tirerClairance() {
    constexpr std::size_t NB = std::size(CLAIRANCES);
    static std::mt19937   rng{std::random_device{}()};
    static std::size_t    precedente = NB; /* NB = aucun tirage encore */

    std::uniform_int_distribution<std::size_t> tirage(0, NB - 1);
    std::size_t i = tirage(rng);
    if (i == precedente) {
        i = (i + 1) % NB;
    }
    precedente = i;
    return CLAIRANCES[i];
}

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

void Application::resetRadioMessage() noexcept {
    m_radioMsgArmed = false;
    m_radioMsgDone  = false;
    m_radioMsgDelay = 0.0f;
    m_radioMsgShow  = 0.0f;  /* pas de sous-titre hérité de la session précédente */
    m_radioMsg.clear();
}

void Application::updateRadioMessage(float turbineFraction, float t, float frameDt) {
    /* Mode zombie : pas d'annonce de la tour. On entre en combat turbine et rotor
       déjà au régime, face à une horde : une autorisation de décollage n'aurait
       pas de sens. Le verrou de rotor qui l'accompagne saute avec elle, sans quoi
       l'appareil resterait cloué au pad en attendant une réplique qui ne vient
       pas (voir la fin de cette fonction). */
    if (m_combat.active()) {
        m_radioMsgShow = 0.0f;  /* ni voix ni sous-titre */
        m_flight.turbine().setRotorHold(false);
        return;
    }

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
            /* Même intention pour une précision entre parenthèses : l'arène de Dax
               s'appelle "Dax-Seyresse (pad est)", que la voix de synthèse lit tel
               quel, parenthèses comprises. La tour annonce le terrain, pas le pad. */
            const std::size_t paren = station.find(" (");
            if (paren != std::string::npos) {
                station = station.substr(0, paren);
            }
            const ClairanceRadio& clairance = tirerClairance();
            m_radioMsg = station.empty() ? "Fox-Bravo, tower, " : "Fox-Bravo, " + station + " tower, ";
            if (clairance.salutation) {
                m_radioMsg += salutationRadio(timeOfDaySeconds(t));
                m_radioMsg += ", ";
            }
            m_radioMsg += clairance.corps;

            /* Durée du sous-titre proportionnelle à la longueur du message : les
               clairances n'ont plus toutes la même taille, et un sous-titre figé
               couperait les plus longues avant la fin de la voix. Le coefficient
               est calé sur le débit de Flite (duration_stretch 0.82), la constante
               laissant le temps de lire après la dernière syllabe. */
            m_radioMsgShow = 3.0f + 0.065f * static_cast<float>(m_radioMsg.size());
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
