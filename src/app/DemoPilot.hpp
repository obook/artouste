/*
 * DemoPilot.hpp
 * Pilote automatique du mode démo : il joue tout seul une démonstration, puis boucle.
 * Mission : décollage du pad, montée et vol jusqu'à la Dune du Pilat qu'il survole
 * haut (environ 1500 m), demi-tour, retour au pad de départ et pose. Le démarrage et
 * la montée initiale sont scriptés ; ensuite un guidage proportionnel léger mène
 * l'appareil vers la cible courante (la dune à l'aller, le pad au retour) : il vise au
 * cap, règle sa vitesse et sa hauteur d'après la distance restante (il monte à
 * l'aller, ralentit et descend pour se poser au retour). La démo se relance une fois
 * l'appareil reposé sur son pad.
 *
 * Le pilote ne fait que produire des commandes : il ne touche ni à la physique ni à
 * la turbine. L'application applique ses commandes, change de vue et déclenche le
 * démarrage rapide de la turbine au lancement de la démo.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/Controls.hpp"
#include "util/Math.hpp"

namespace artouste::app {

class DemoPilot {
public:
    /* Ce que la démo demande à l'application à chaque image. */
    struct Output {
        physics::Controls controls;            /* commandes à appliquer à l'appareil */
        int               viewMode   = 2;      /* vue souhaitée (0 poursuite, 1 cockpit, 2 orbite) */
        bool              cutTurbine = false;  /* couper la turbine (une fois posé) */
        bool              finished   = false;  /* séquence d'arrêt terminée : relancer la démo */
    };

    /* (Re)lance la démo au temps zéro. returnPad est le pad de départ et d'arrivée (on
       décolle de là et on y revient se poser) ; dunePoint est le point à survoler (la
       Dune du Pilat). Tous deux en coordonnées monde (x est, z sud, y au sol). */
    void start(const vec3& returnPad, const vec3& dunePoint) noexcept;

    /* Avance la démo d'un pas de temps et renvoie les commandes à appliquer.
       position et velocity sont l'état courant de l'appareil (repère monde) ; heading
       est le cap (lacet, rad) au sens de l'application (atan2(-forward.z, forward.x)) ;
       groundHeight est l'altitude du sol sous l'appareil ; rotorFraction est le régime
       du rotor [0, 1] : on attend qu'il soit plein avant de décoller. */
    Output update(float dt, const vec3& position, const vec3& velocity, float heading,
                  float groundHeight, float rotorFraction) noexcept;

    [[nodiscard]] bool active() const noexcept { return m_active; }
    void stop() noexcept { m_active = false; }

private:
    /* Fait évoluer le collectif lissé vers la valeur visée à vitesse bornée, pour un
       maniement progressif du levier (décollage et manoeuvres en douceur). */
    float rampeCollectif(float cible, float dt) noexcept;

    bool  m_active        = false;
    float m_elapsed       = 0.0f;   /* secondes écoulées depuis le début de la démo */
    float m_rotorReadyTime = -1.0f; /* instant (m_elapsed) où le rotor a atteint son régime, < 0 tant qu'on attend */
    bool  m_retour        = false;  /* étape : false = aller (vers la dune), true = retour (vers le pad) */
    float m_collective    = 0.0f;   /* collectif lissé (monte/descend progressivement) */
    bool  m_landed        = false;  /* posé sur le pad : on entame la séquence d'arrêt */
    bool  m_turbineCut    = false;  /* la coupure de la turbine a déjà été demandée */
    float m_stoppedTime   = -1.0f;  /* instant (m_elapsed) où le rotor s'est arrêté, < 0 sinon */

    vec3 m_returnPad{0.0f};  /* pad de départ et d'arrivée */
    vec3 m_dunePoint{0.0f};  /* point à survoler (Dune du Pilat) */
};

}  /* namespace artouste::app */
