/*
 * Gamepad.hpp
 * Lecture des commandes à la manette via l'API gamepad de GLFW (mapping SDL).
 * Zone morte et courbe d'expansion adoucissent les sticks. Répartition :
 *   stick gauche  -> cyclique (tangage / roulis)
 *   stick droit X -> palonniers
 *   gâchette droite -> collectif
 * Aucun état conservé : on lit directement la manette à chaque appel.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include "physics/Controls.hpp"

namespace artouste::input {

class Gamepad {
public:
    /* Manette branchée ET reconnue (mapping SDL disponible) ? */
    [[nodiscard]] static bool isPresent() noexcept;

    /* Une commande dépasse-t-elle la zone morte ? Sert à savoir quelle
     * source de commande est utilisée. */
    [[nodiscard]] static bool isActive() noexcept;

    /* Lit l'état courant. Renvoie des commandes neutres si la manette est absente. */
    [[nodiscard]] static physics::Controls poll() noexcept;
};

}  /* namespace artouste::input */
