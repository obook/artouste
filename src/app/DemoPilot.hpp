/*
 * DemoPilot.hpp
 * Pilote automatique du mode démo : il joue tout seul une démonstration, puis boucle.
 * Mission : décollage du pad, puis survol d'une suite de points de passage, chacun à
 * sa propre altitude (par exemple le cap Ferret à 500 m puis Arcachon à 1000 m) ;
 * après le dernier point, retour au pad de départ et pose. Le démarrage et la montée
 * initiale sont scriptés ; ensuite un guidage proportionnel léger mène l'appareil vers
 * le point courant : il vise au cap, règle sa vitesse et sa hauteur d'après la distance
 * restante. Une fois assez proche d'un point, il vise le suivant ; après le dernier, il
 * ralentit et descend pour se poser au pad. La démo se relance une fois l'appareil
 * reposé sur son pad.
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

#include <cstddef>
#include <vector>

namespace artouste::app {

class DemoPilot {
public:
    /* Un point de passage : un lieu à survoler (coordonnées monde) et la hauteur de
       survol visée à cet endroit (m). */
    struct Waypoint {
        vec3  point{0.0f};
        float altitude = 0.0f;
    };

    /* Ce que la démo demande à l'application à chaque image. */
    struct Output {
        physics::Controls controls;            /* commandes à appliquer à l'appareil */
        int               viewMode   = 2;      /* vue souhaitée (0 poursuite, 1 cockpit, 2 orbite) */
        int               hudStyle   = 0;      /* HUD souhaité (0 aucun, 1 complet, 2 quatre coins) */
        bool              cutTurbine = false;  /* couper la turbine (une fois posé) */
        bool              finished   = false;  /* séquence d'arrêt terminée : relancer la démo */
    };

    /* (Re)lance la démo au temps zéro. returnPad est le pad de départ et d'arrivée (on
       décolle de là et on y revient se poser) ; route est la suite des points à survoler,
       dans l'ordre, chacun avec sa hauteur de survol. Tout en coordonnées monde (x est,
       z sud, y au sol). Si la route est vide, la démo se contente d'un décollage et d'une
       pose. */
    void start(const vec3& returnPad, const std::vector<Waypoint>& route) noexcept;

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
    bool  m_returning     = false;  /* étape : false = on enchaîne les points, true = retour au pad */
    std::size_t m_index   = 0;      /* indice du point de passage courant (tant qu'on n'est pas au retour) */
    float m_collective    = 0.0f;   /* collectif lissé (monte/descend progressivement) */
    bool  m_landed        = false;  /* posé sur le pad : on entame la séquence d'arrêt */
    bool  m_turbineCut    = false;  /* la coupure de la turbine a déjà été demandée */
    float m_stoppedTime   = -1.0f;  /* instant (m_elapsed) où le rotor s'est arrêté, < 0 sinon */

    vec3                  m_returnPad{0.0f};  /* pad de départ et d'arrivée */
    std::vector<Waypoint> m_route;            /* points à survoler, dans l'ordre */
};

}  /* namespace artouste::app */
