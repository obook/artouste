/*
 * InputSystem.cpp
 * Choix de la source de commandes (clavier ou manette) selon la dernière
 * activité, et restitution des commandes correspondantes.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "input/InputSystem.hpp"

#include "input/Gamepad.hpp"

namespace artouste::input {

physics::Controls InputSystem::poll(float dt) noexcept {
    /* On lit toujours le clavier pour garder le collectif à jour, même quand
     * la manette a la main : le retour au clavier se fait ainsi sans à-coup. */
    const physics::Controls keyboardControls = m_keyboard.poll(dt);

    /* On bascule de source dès qu'une nouvelle activité est détectée. */
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

}  /* namespace artouste::input */
