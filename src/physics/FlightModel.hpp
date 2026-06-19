/*
 * FlightModel.hpp
 * Vrai modèle de vol : il rassemble toutes les forces et tous les couples qui
 * agissent sur l'hélicoptère (poussée du rotor, gravité, traînée, commandes,
 * anti-couple, amortissements), puis les confie au corps rigide pour avancer.
 *
 * Auteur : O. Booklage
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

    [[nodiscard]] const RigidBody& body() const noexcept { return m_body; }

    /* Dernière poussée calculée, en newtons : utile pour le débogage et l'affichage. */
    [[nodiscard]] float lastThrust() const noexcept { return m_lastThrust; }

private:
    RigidBody m_body;
    float     m_lastThrust = 0.0f;
};

}  /* namespace artouste::physics */
