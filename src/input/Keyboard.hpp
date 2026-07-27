/*
 * Keyboard.hpp
 * Lecture des commandes de vol au clavier.
 * Le cyclique et les palonniers reviennent doucement au neutre ; le
 * collectif garde sa position, comme le vrai levier.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/Controls.hpp"

struct GLFWwindow;

namespace artouste::input {

/* Jeton GLFW de la touche qui IMPRIME cette lettre sur le clavier de
   l'utilisateur.

   Les jetons GLFW désignent des POSITIONS physiques, définies sur une
   disposition US : GLFW_KEY_A est la touche en haut à gauche, celle qui écrit
   "a" en QWERTY mais "q" en AZERTY. Un raccourci écrit en dur avec ces jetons ne
   tombe donc sur la bonne touche que pour une partie des utilisateurs. On
   demande ici à GLFW le nom réel de chaque touche, ce qui donne la disposition
   effective, quelle qu'elle soit (AZERTY, QWERTZ, Dvorak...).

   Renvoie le jeton US correspondant si la disposition n'est pas connue de GLFW,
   ce qui ne fait pas pire qu'avant. La lettre doit être minuscule et non
   accentuée. Demande GLFW initialisé. */
[[nodiscard]] int toucheImprimant(char lettre);

class Keyboard {
public:
    explicit Keyboard(GLFWwindow* window) noexcept : m_window(window) {}

    /* Met à jour et renvoie les commandes pour ce pas de temps. */
    physics::Controls poll(float dt) noexcept;

    /* Vrai si au moins une touche de pilotage est enfoncée : sert à savoir
     * quelle source de commande est utilisée. */
    [[nodiscard]] bool isActive() const noexcept;

    /* Ctrl gauche est-elle actuellement tenue ? État maintenu (pas un front
     * montant) : équivalent clavier de Gamepad::fireHeld, mode zombie
     * uniquement. */
    [[nodiscard]] bool fireHeld() const noexcept;

    void reset() noexcept { m_controls = physics::Controls{}; }

    /* Recale le collectif mémorisé sur value, sans toucher au cyclique ni aux
       palonniers (qui reviennent seuls au neutre). Sert à resynchroniser le levier
       tenu par le clavier avec le collectif réel après un vol automatique (mode
       démo ou atterrissage automatique) : sans cela, le levier resterait sur sa
       position d'avant l'engagement et l'appareil sauterait dessus à la reprise
       en main. */
    void setCollective(float value) noexcept { m_controls.collective = value; }

private:
    GLFWwindow*       m_window = nullptr;
    physics::Controls m_controls;
};

}  /* namespace artouste::input */
