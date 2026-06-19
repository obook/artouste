// Modèle de vol v1 (M3) : assemble les forces et moments (poussée, gravité,
// traînée, cyclique, anti-couple, amortissements) et les intègre via RigidBody.
// Décollage et vol stationnaire jouables, sans effets fins (effet de sol,
// translation, autorotation arrivent plus tard). Aucune dépendance au rendu.

#pragma once

#include "physics/Controls.hpp"
#include "physics/RigidBody.hpp"

namespace artouste::physics {

class FlightModel {
public:
    // Avance la simulation d'un pas dt (appelé à cadence fixe par l'appelant).
    void update(const Controls& controls, float dt) noexcept;

    void reset() noexcept { m_body = RigidBody{}; }

    // Réinitialise à une altitude donnée (pratique pour tester hors effet de sol).
    void reset(float altitude) noexcept {
        m_body            = RigidBody{};
        m_body.position.y = altitude;
    }

    [[nodiscard]] const RigidBody& body() const noexcept { return m_body; }

    // Dernière poussée calculée (N) : utile au débogage et au futur HUD.
    [[nodiscard]] float lastThrust() const noexcept { return m_lastThrust; }

private:
    RigidBody m_body;
    float     m_lastThrust = 0.0f;
};

}  // namespace artouste::physics
