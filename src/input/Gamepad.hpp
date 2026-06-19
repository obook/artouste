/*
 * Gamepad.hpp
 * Lecture des commandes à la manette (API gamepad de GLFW / mapping SDL).
 * Répartition des commandes :
 *   stick gauche    -> cyclique (tangage / roulis)
 *   stick droit X   -> palonniers
 *   gâchettes RT/LT -> collectif (levier : RT monte, LT descend, garde la position)
 *   bouton B        -> change de vue
 * La manette conserve un état d'une image à l'autre : la position du levier de
 * collectif et l'état précédent du bouton B (pour détecter le moment de l'appui).
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

    /* La manette est-elle nettement sollicitée ? Sert à choisir la source de
     * commande active (clavier ou manette). */
    [[nodiscard]] static bool isActive() noexcept;

    /* Met à jour et renvoie les commandes pour ce pas de temps (dt en secondes).
     * Le collectif est un levier : il garde sa position quand on relâche. */
    [[nodiscard]] physics::Controls poll(float dt) noexcept;

    /* Vrai une seule fois, au moment où le bouton B vient d'être pressé. */
    [[nodiscard]] bool viewTogglePressed() noexcept;

    /* Remet le levier de collectif à zéro. */
    void reset() noexcept { m_collective = 0.0f; }

private:
    float m_collective = 0.0f;  /* position mémorisée du levier de collectif */
    bool  m_prevB      = false; /* état du bouton B à l'image précédente */
};

}  /* namespace artouste::input */
