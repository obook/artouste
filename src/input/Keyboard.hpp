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

class Keyboard {
public:
    explicit Keyboard(GLFWwindow* window) noexcept : m_window(window) {}

    /* Met à jour et renvoie les commandes pour ce pas de temps. */
    physics::Controls poll(float dt) noexcept;

    /* Vrai si au moins une touche de pilotage est enfoncée : sert à savoir
     * quelle source de commande est utilisée. */
    [[nodiscard]] bool isActive() const noexcept;

    void reset() noexcept { m_controls = physics::Controls{}; }

private:
    GLFWwindow*       m_window = nullptr;
    physics::Controls m_controls;
};

}  /* namespace artouste::input */
