/*
 * InputSystem.hpp
 * Fusionne clavier et manette en une seule source de commandes.
 * On retient la dernière source active pour ne pas mélanger deux pilotages
 * et afficher l'aide visuelle adaptée.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include "input/Keyboard.hpp"
#include "physics/Controls.hpp"

struct GLFWwindow;

namespace artouste::input {

enum class Source { Keyboard, Gamepad };

class InputSystem {
public:
    explicit InputSystem(GLFWwindow* window) noexcept : m_keyboard(window) {}

    physics::Controls poll(float dt) noexcept;
    void              reset() noexcept { m_keyboard.reset(); }

    [[nodiscard]] Source activeSource() const noexcept { return m_active; }

private:
    Keyboard m_keyboard;
    Source   m_active = Source::Keyboard;
};

}  /* namespace artouste::input */
