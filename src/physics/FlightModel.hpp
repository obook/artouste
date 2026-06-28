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
    }

    /* Réinitialise à une altitude donnée, pratique pour tester loin du sol. */
    void reset(float altitude) noexcept {
        m_body            = RigidBody{};
        m_body.position.y = altitude;
        m_fuelLiters      = FUEL_CAPACITY_L;
    }

    /* Réinitialise à une position donnée (par exemple posé sur la côte). */
    void reset(const vec3& position) noexcept {
        m_body          = RigidBody{};
        m_body.position = position;
        m_fuelLiters    = FUEL_CAPACITY_L;
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

private:
    RigidBody m_body;
    Turbine   m_turbine;
    float     m_lastThrust   = 0.0f;
    float     m_groundHeight = 0.0f;
    float     m_fuelLiters   = FUEL_CAPACITY_L;
    bool      m_realFlyPhysicsEnabled = true;  /* coupé en mode assisté et en démo */
};

}  /* namespace artouste::physics */
