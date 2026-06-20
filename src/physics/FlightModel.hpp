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

namespace artouste::physics {

class FlightModel {
public:
    /* Avance la simulation d'un pas de durée dt (appelé à cadence régulière). */
    void update(const Controls& controls, float dt) noexcept;

    void reset() noexcept { m_body = RigidBody{}; }

    /* Réinitialise à une altitude donnée, pratique pour tester loin du sol. */
    void reset(float altitude) noexcept {
        m_body            = RigidBody{};
        m_body.position.y = altitude;
    }

    /* Réinitialise à une position donnée (par exemple posé sur la côte). */
    void reset(const vec3& position) noexcept {
        m_body          = RigidBody{};
        m_body.position = position;
    }

    /* Altitude du sol (m) sous l'appareil : le contact se fait à cette hauteur
       plutôt qu'au niveau de la mer, pour suivre le relief du terrain. */
    void setGroundHeight(float h) noexcept { m_groundHeight = h; }

    [[nodiscard]] const RigidBody& body() const noexcept { return m_body; }

    /* Dernière poussée calculée, en newtons : utile pour le débogage et l'affichage. */
    [[nodiscard]] float lastThrust() const noexcept { return m_lastThrust; }

private:
    RigidBody m_body;
    float     m_lastThrust   = 0.0f;
    float     m_groundHeight = 0.0f;
};

}  /* namespace artouste::physics */
