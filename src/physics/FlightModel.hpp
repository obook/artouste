/*
 * FlightModel.hpp
 * Vrai modèle de vol : il rassemble toutes les forces et tous les couples qui
 * agissent sur l'hélicoptère (poussée du rotor, gravité, traînée, commandes,
 * anti-couple, amortissements), puis les confie au corps rigide pour avancer.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/Controls.hpp"
#include "physics/RigidBody.hpp"
#include "physics/Turbine.hpp"
#include "physics/constants.hpp"

#include <algorithm>

namespace artouste::physics {

class FlightModel {
public:
    /* Avance la simulation d'un pas de durée dt (appelé à cadence régulière). */
    void update(const Controls& controls, float dt) noexcept;

    /* Turbine Artouste : on l'expose pour la piloter (démarrage/arrêt) et lire
     * son état (HUD, audio). Le modèle multiplie poussée et anti-couple par son
     * régime ; il faut donc la démarrer pour décoller. Repositionner l'appareil
     * (reset) ne touche pas à la turbine : elle garde son état. */
    [[nodiscard]] Turbine&       turbine() noexcept { return m_turbine; }
    [[nodiscard]] const Turbine& turbine() const noexcept { return m_turbine; }

    /* Repositionner l'appareil refait le plein (retour à la base) ; la turbine,
       elle, garde son état. */
    void reset() noexcept {
        m_body      = RigidBody{};
        m_fuelLiters = FUEL_CAPACITY_L;
        clearGroundImpact();
    }

    /* Réinitialise à une altitude donnée, pratique pour tester loin du sol. */
    void reset(float altitude) noexcept {
        m_body            = RigidBody{};
        m_body.position.y = altitude;
        m_fuelLiters      = FUEL_CAPACITY_L;
        clearGroundImpact();
    }

    /* Réinitialise à une position donnée (par exemple posé sur la côte). */
    void reset(const vec3& position) noexcept {
        m_body          = RigidBody{};
        m_body.position = position;
        m_fuelLiters    = FUEL_CAPACITY_L;
        clearGroundImpact();
    }

    /* Réinitialise à une position et un cap donnés (degrés boussole : 0 = nord,
       90 = est, 180 = sud). L'axe avant du corps est +X, soit l'est à l'identité ;
       on tourne donc autour de la verticale de (90 - cap) degrés. */
    void reset(const vec3& position, float headingDeg) noexcept {
        reset(position);
        m_body.orientation = glm::angleAxis(glm::radians(90.0f - headingDeg),
                                            vec3{0.0f, 1.0f, 0.0f});
    }

    /* Altitude du sol (m) sous l'appareil : le contact se fait à cette hauteur
       plutôt qu'au niveau de la mer, pour suivre le relief du terrain. */
    void setGroundHeight(float h) noexcept { m_groundHeight = h; }

    /* Active ou non les difficultés de pilotage (perte de puissance en altitude,
       traînée d'onde au-delà de la VNE, perte d'autorité au palonnier en vol
       latéral, vortex ring state). On les coupe en mode assisté et en démo, pour
       garder un vol facile et prévisible. Les effets qui aident le pilote (effet de
       sol, effet de translation) restent toujours actifs. */
    void setRealFlyPhysicsEnabled(bool enabled) noexcept { m_realFlyPhysicsEnabled = enabled; }

    [[nodiscard]] const RigidBody& body() const noexcept { return m_body; }

    /* Dernière poussée calculée, en newtons : utile pour le débogage et l'affichage. */
    [[nodiscard]] float lastThrust() const noexcept { return m_lastThrust; }

    /* Carburant restant, en litres (pour le HUD et le voyant d'alerte). */
    [[nodiscard]] float fuelLiters() const noexcept { return m_fuelLiters; }

    /* Démarre ou coupe la turbine, comme Turbine::toggle, mais en tenant compte du
       réservoir : un démarrage est REFUSÉ sous FUEL_START_MIN_L, une coupure est
       toujours acceptée. Rend vrai si l'état a changé.

       Sans ce garde-fou, appuyer sur le démarreur à sec lançait la séquence et son
       vacarme d'une minute, pour une extinction juste avant le régime de vol. Le
       cas le plus traître n'est même pas le réservoir vide (coupé au premier pas de
       simulation) mais le fond de réservoir : la jauge affiche des litres entiers,
       donc "0 L" peut cacher un demi-litre, de quoi amorcer un démarrage
       parfaitement inutile. */
    bool toggleTurbine() noexcept {
        const bool aLArret = m_turbine.state() == Turbine::State::Arret ||
                             m_turbine.state() == Turbine::State::Extinction;
        if (aLArret && m_fuelLiters < FUEL_START_MIN_L) {
            return false; /* pas assez de carburant pour mener un démarrage à terme */
        }
        m_turbine.toggle();
        return true;
    }

    /* Vide une quantité de carburant hors consommation de la turbine : une
       cellule qui encaisse un choc perd du kérosène (voir le contact avec le sol
       du mode zombie). Le réservoir ne descend jamais sous zéro, et une quantité
       nulle ou négative ne fait rien. */
    void drainFuel(float liters) noexcept {
        if (liters > 0.0f) {
            m_fuelLiters = std::max(0.0f, m_fuelLiters - liters);
        }
    }

    /* Intensité du vortex ring state (0 = aucun, 1 = plein), pour l'alerte HUD.
       Toujours 0 en mode assisté et en démo (physique réelle coupée). */
    [[nodiscard]] float vrsIntensity() const noexcept { return m_vrsIntensity; }

    /* Vitesse d'arrivée (m/s) du dernier contact avec le sol, puis remise à zéro :
       l'appelant la lit une fois et la consomme. Elle ne peut se mesurer QU'ICI,
       le contact annulant aussitôt la composante verticale (voir update) ; la
       boucle de jeu, qui tourne bien plus lentement que la simulation, ne verrait
       plus qu'un appareil posé, vitesse nulle. Le mode zombie en tire le carburant
       que fait fuir un posé brutal. Vaut le maximum des contacts survenus depuis la dernière
       lecture, et reste à 0 tant que l'appareil demeure au sol. */
    [[nodiscard]] float consumeGroundImpact() noexcept {
        const float v    = m_groundImpactMs;
        m_groundImpactMs = 0.0f;
        return v;
    }

private:
    /* Repositionner l'appareil ne doit pas laisser derrière lui un contact non
       lu : la partie suivante encaisserait les dégâts d'un posé qui n'a pas eu
       lieu. On oublie aussi l'état "au sol", le nouveau point de départ pouvant
       être en vol comme sur un pad. */
    void clearGroundImpact() noexcept {
        m_groundImpactMs  = 0.0f;
        m_inGroundContact = false;
    }

    RigidBody m_body;
    Turbine   m_turbine;
    float     m_lastThrust   = 0.0f;
    float     m_groundHeight = 0.0f;
    float     m_fuelLiters   = FUEL_CAPACITY_L;
    float     m_vrsIntensity  = 0.0f;          /* vortex ring state, 0..1 (alerte HUD) */
    float     m_groundImpactMs = 0.0f;         /* vitesse du dernier contact, non lue */
    bool      m_inGroundContact = false;       /* déjà au sol au pas précédent */
    bool      m_realFlyPhysicsEnabled = true;  /* coupé en mode assisté et en démo */
};

}  /* namespace artouste::physics */
