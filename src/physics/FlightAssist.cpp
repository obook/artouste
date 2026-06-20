/*
 * FlightAssist.cpp
 * Mise en oeuvre du mode assisté. Toutes les corrections sont pondérées par
 * l'intensité (0 a 1), qui monte ou descend progressivement a chaque bascule,
 * pour que le passage assisté <-> brut ne provoque aucun saut de comportement.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "physics/FlightAssist.hpp"

#include "physics/constants.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::physics {

void FlightAssist::syncTo(const Controls& raw) noexcept {
    m_cyclicLateral      = raw.cyclicLateral;
    m_cyclicLongitudinal = raw.cyclicLongitudinal;
    m_pedals             = raw.pedals;
    m_collective         = raw.collective;
}

Controls FlightAssist::apply(const Controls& raw, float dt) noexcept {
    /* Transition progressive de l'intensité vers sa cible (1 si l'assistance est
     * demandée, 0 sinon), a vitesse constante : la bascule prend environ une demi-seconde. */
    const float target = m_active ? 1.0f : 0.0f;
    const float step    = ASSIST_TRANSITION_RATE * dt;
    m_intensity += clamp(target - m_intensity, -step, step);

    /* Assistance complètement repliée : on transmet les commandes brutes telles
     * quelles et on garde l'état calé dessus, pour repartir sans à-coup au prochain coup. */
    if (m_intensity <= 0.0f) {
        syncTo(raw);
        return raw;
    }

    /* 1. Limitation du taux de variation du collectif : la consigne ne peut pas
     *    grimper ni chuter plus vite que ASSIST_COLLECTIVE_RATE par seconde, ce qui
     *    évite les coups de puissance brutaux (et les sauts de lacet qu'ils causent). */
    const float maxDelta   = ASSIST_COLLECTIVE_RATE * dt;
    const float collective = m_collective + clamp(raw.collective - m_collective, -maxDelta, maxDelta);
    const float deltaColl  = collective - m_collective;  /* variation réellement appliquée cette image */

    /* 2. Lissage des axes du cyclique vers la consigne du pilote. En l'absence
     *    d'input, la consigne devient le neutre et un lissage plus doux ramène
     *    le manche au centre (rappel progressif, pas de retour brutal). */
    const bool cyclicActif = std::fabs(raw.cyclicLateral) > ASSIST_INPUT_DEADZONE ||
                             std::fabs(raw.cyclicLongitudinal) > ASSIST_INPUT_DEADZONE;
    const float cycLatTarget  = cyclicActif ? raw.cyclicLateral : 0.0f;
    const float cycLongTarget = cyclicActif ? raw.cyclicLongitudinal : 0.0f;
    const float cycTau        = cyclicActif ? ASSIST_SMOOTH_TAU : ASSIST_RECENTER_TAU;

    m_cyclicLateral      = lowPass(m_cyclicLateral,      cycLatTarget,  dt, cycTau);
    m_cyclicLongitudinal = lowPass(m_cyclicLongitudinal, cycLongTarget, dt, cycTau);

    /* 3. Lissage du palonnier vers la consigne du pilote. */
    m_pedals = lowPass(m_pedals, raw.pedals, dt, ASSIST_SMOOTH_TAU);

    /* 4. Compensation automatique du lacet : quand le collectif varie, le couple
     *    rotor fait partir le nez ; on pousse le palonnier dans le sens correctif,
     *    proportionnellement a la variation de collectif. */
    const float pedalsAssisted = clamp(m_pedals + deltaColl * ASSIST_ANTITORQUE_GAIN, -1.0f, 1.0f);

    /* On valide l'état du collectif lissé pour l'image suivante. */
    m_collective = collective;

    /* Mélange entre commandes brutes et commandes assistées selon l'intensité :
     * c'est ce qui rend la bascule entre les deux modes parfaitement continue. */
    Controls out;
    out.cyclicLateral      = lerp(raw.cyclicLateral,      m_cyclicLateral,      m_intensity);
    out.cyclicLongitudinal = lerp(raw.cyclicLongitudinal, m_cyclicLongitudinal, m_intensity);
    out.collective         = lerp(raw.collective,         collective,           m_intensity);
    out.pedals             = lerp(raw.pedals,             pedalsAssisted,       m_intensity);
    return out;
}

}  /* namespace artouste::physics */
