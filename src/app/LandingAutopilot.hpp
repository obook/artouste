/*
 * LandingAutopilot.hpp
 * Atterrissage automatique déclenchable en vol libre (touche J / croix bas) :
 * guide l'appareil vers un pad donné et se pose en douceur, puis rend la main.
 * Reprend le guidage d'approche et de pose de DemoPilot (voir DemoPilotDetail.hpp),
 * sans la route à plusieurs points ni la séquence d'arrêt turbine propres à la démo :
 * ici la turbine reste en régime, prête pour un nouveau décollage.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/Controls.hpp"
#include "util/Math.hpp"

namespace artouste::app {

class LandingAutopilot {
public:
    /* Engage l'atterrissage automatique vers target (position monde du pad visé).
       initialControls sont les commandes brutes du pilote au moment de l'engagement :
       leur collectif sert de point de départ de la rampe du levier (pas de à-coup),
       et l'ensemble sert de référence pour détecter une reprise de main (voir
       baseline). */
    void start(const vec3& target, const physics::Controls& initialControls) noexcept;

    void stop() noexcept { m_active = false; }

    [[nodiscard]] bool active() const noexcept { return m_active; }

    /* Commandes brutes du pilote au moment de l'engagement. Le cyclique revient
       normalement au neutre quand on relâche le manche, mais rien n'oblige le
       pilote à l'avoir relâché pile à l'instant de l'engagement (il peut être en
       train de corriger une trajectoire quand il déclenche l'atterrissage
       automatique) ; le collectif, lui, est un levier qui garde toujours sa
       position (jamais neutre au repos). Comparer les commandes courantes du
       pilote à cette référence (plutôt qu'au neutre) permet de détecter qu'il a
       vraiment bougé une commande depuis l'engagement, sans faux positif dû à sa
       position au moment du déclenchement. */
    [[nodiscard]] const physics::Controls& baseline() const noexcept { return m_baseline; }

    /* Avance l'atterrissage d'un pas de temps et renvoie les commandes à appliquer.
       position et velocity sont l'état courant de l'appareil (repère monde) ; heading
       est le cap (lacet, rad) ; agl est la hauteur au-dessus du sol sous l'appareil.
       Se désengage de lui-même (active() devient faux) une fois le collectif ramené
       au sol (voir m_grounded). */
    physics::Controls update(float dt, const vec3& position, const vec3& velocity,
                             float heading, float agl) noexcept;

private:
    float rampeCollectif(float cible, float dt) noexcept;

    bool               m_active     = false;
    float              m_collective = 0.0f;  /* collectif lissé (monte/descend progressivement) */
    physics::Controls  m_baseline{};
    vec3               m_target{0.0f};

    /* Vrai dès le contact avec le pad : on ramène alors le collectif à zéro (au lieu
       de rendre la main directement) pour sortir de l'effet de sol, sans quoi le
       collectif de pose (proche de la sustentation) ferait redécoller l'appareil. */
    bool  m_grounded = false;
};

}  /* namespace artouste::app */
