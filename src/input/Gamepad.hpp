// Lecture des commandes à la manette. Utilise l'API gamepad
// de GLFW (mapping SDL), avec zone morte et courbe d'expansion sur les sticks.
//   Stick gauche  -> cyclique (tangage/roulis)
//   Stick droit X -> palonniers
//   Gâchettes LT/RT -> collectif
// Sans état : on lit directement l'état courant de la manette.

#pragma once

#include "physics/Controls.hpp"

namespace artouste::input {

class Gamepad {
public:
    // Manette branchée ET reconnue (mapping SDL disponible) ?
    [[nodiscard]] static bool isPresent() noexcept;

    // Une commande au-delà de la zone morte est-elle active ? (détection de source)
    [[nodiscard]] static bool isActive() noexcept;

    // Lit l'état courant. Renvoie des commandes neutres si absente.
    [[nodiscard]] static physics::Controls poll() noexcept;
};

}  // namespace artouste::input
