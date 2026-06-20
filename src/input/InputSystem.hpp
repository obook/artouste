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

    /* Le bouton Start de la manette vient-il d'être pressé ? (démarre/coupe la turbine) */
    [[nodiscard]] bool turbineTogglePressed() noexcept { return m_gamepad.turbineTogglePressed(); }

    /* Le bouton B de la manette vient-il d'être pressé ? (affiche/masque le HUD) */
    [[nodiscard]] bool hudTogglePressed() noexcept { return m_gamepad.hudTogglePressed(); }

    /* Le bouton Back de la manette vient-il d'être pressé ? (pause/reprise) */
    [[nodiscard]] bool pauseTogglePressed() noexcept { return m_gamepad.pauseTogglePressed(); }

    /* Le bouton X de la manette vient-il d'être pressé ? (reset position) */
    [[nodiscard]] bool resetPressed() noexcept { return m_gamepad.resetPressed(); }

    /* La combinaison LB + RB vient-elle d'être pressée ? (quitter) */
    [[nodiscard]] bool quitPressed() noexcept { return m_gamepad.quitPressed(); }

    /* Le bouton A vient-il d'être pressé ? (bascule la livrée Gendarmerie) */
    [[nodiscard]] bool liveryTogglePressed() noexcept { return m_gamepad.liveryTogglePressed(); }

    /* La croix directionnelle haut vient-elle d'être pressée ? (bascule le mode assisté) */
    [[nodiscard]] bool assistTogglePressed() noexcept { return m_gamepad.assistTogglePressed(); }

    [[nodiscard]] Source activeSource() const noexcept { return m_active; }

private:
    Keyboard m_keyboard;
    Gamepad  m_gamepad;
    Source   m_active = Source::Keyboard;
};

}  /* namespace artouste::input */
