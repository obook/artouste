// Vol "arcade" du jalon M2 : pas de physique réaliste, contrôle direct.
// Les commandes pilotent directement les vitesses (lacet, montée, translation)
// et l'assiette n'est qu'un retour visuel. Sert à valider entrées et caméra
// avant le vrai modèle de vol (M3). Aucune dépendance au rendu : testable seul.

#pragma once

#include "physics/Controls.hpp"
#include "physics/FlightState.hpp"

namespace artouste::physics {

class ArcadeFlight {
public:
    void update(const Controls& controls, float dt) noexcept;
    void reset() noexcept { m_state = FlightState{}; }

    [[nodiscard]] const FlightState& state() const noexcept { return m_state; }

private:
    FlightState m_state;
};

}  // namespace artouste::physics
