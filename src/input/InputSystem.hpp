/*
 * InputSystem.hpp
 * Fusionne clavier et manette en une seule source de commandes.
 * On retient la dernière source active pour ne pas mélanger deux pilotages
 * et afficher l'aide visuelle adaptée.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "input/Gamepad.hpp"
#include "input/Keyboard.hpp"
#include "physics/Controls.hpp"

struct GLFWwindow;

namespace artouste::input {

enum class Source { Keyboard, Gamepad };

class InputSystem {
public:
    explicit InputSystem(GLFWwindow* window) noexcept : m_keyboard(window) {}

    physics::Controls poll(float dt) noexcept;

    void reset() noexcept {
        m_keyboard.reset();
        m_gamepad.reset();
    }

    /* Le bouton Y de la manette vient-il d'être pressé ? (change de vue) */
    [[nodiscard]] bool viewTogglePressed() noexcept { return m_gamepad.viewTogglePressed(); }

    [[nodiscard]] Source activeSource() const noexcept { return m_active; }

private:
    Keyboard m_keyboard;
    Gamepad  m_gamepad;
    Source   m_active = Source::Keyboard;
};

}  /* namespace artouste::input */
