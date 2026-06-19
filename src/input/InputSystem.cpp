#include "input/InputSystem.hpp"

#include "input/Gamepad.hpp"

namespace artouste::input {

physics::Controls InputSystem::poll(float dt) noexcept {
    // On lit toujours le clavier pour garder le collectif à jour, même quand la
    // manette a la main : le retour au clavier se fait alors sans à-coup.
    const physics::Controls keyboardControls = m_keyboard.poll(dt);

    // Bascule de source sur la dernière activité détectée.
    if (Gamepad::isActive()) {
        m_active = Source::Gamepad;
    } else if (m_keyboard.isActive()) {
        m_active = Source::Keyboard;
    }

    if (m_active == Source::Gamepad && Gamepad::isPresent()) {
        return Gamepad::poll();
    }
    return keyboardControls;
}

}  // namespace artouste::input
