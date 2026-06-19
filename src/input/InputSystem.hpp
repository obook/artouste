// Fusionne clavier et manette en une seule source de commandes. Les deux sont
// traités en parallèle ; on retient la dernière source active
// pour éviter de mélanger deux pilotages et exposer l'aide visuelle adéquate.

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

}  // namespace artouste::input
