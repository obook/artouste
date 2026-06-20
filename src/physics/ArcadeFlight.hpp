/*
 * ArcadeFlight.hpp
 * Vol "arcade" simplifié : pas de physique réaliste, le pilote agit en direct.
 * Les commandes fixent directement les vitesses (lacet, montée, déplacement) et
 * l'assiette n'est qu'un effet visuel. Sert à tester les entrées et la caméra
 * avant le vrai modèle de vol.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

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

}  /* namespace artouste::physics */
