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

    void primeButtons() noexcept { m_gamepad.primeButtons(); }

    void reset() noexcept {
        m_keyboard.reset();
        m_gamepad.reset();
    }

    /* Recale le collectif mémorisé (clavier et manette) sur value, sans toucher au
       reste des commandes. À utiliser quand un pilote automatique rend la main après
       avoir piloté seul le collectif (le levier réel n'a pas suivi), pour que la
       reprise en main ne fasse pas sauter l'appareil sur sa position d'avant. */
    void syncCollective(float value) noexcept {
        m_keyboard.setCollective(value);
        m_gamepad.setCollective(value);
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
    [[nodiscard]] bool menuPressed() noexcept { return m_gamepad.menuPressed(); }

    /* Le bouton A vient-il d'être pressé ? (fait défiler la livrée) */
    [[nodiscard]] bool liveryTogglePressed() noexcept { return m_gamepad.liveryTogglePressed(); }

    /* LB (L1) vient-elle d'être pressée seule ? (bascule le mode assisté) */
    [[nodiscard]] bool assistTogglePressed() noexcept { return m_gamepad.assistTogglePressed(); }

    /* RB (R1) vient-elle d'être pressée seule ? (bascule l'atterrissage automatique) */
    [[nodiscard]] bool autolandTogglePressed() noexcept { return m_gamepad.autolandTogglePressed(); }

    /* R3 (manette) ou Ctrl gauche (clavier) sont-ils actuellement tenus ? État
       maintenu, mode zombie uniquement (voir CombatMode/Weapon). */
    [[nodiscard]] bool fireHeld() const noexcept {
        return m_gamepad.fireHeld() || m_keyboard.fireHeld();
    }

    /* La croix droite vient-elle d'être pressée ? (radio internet on/off, comme K) */
    [[nodiscard]] bool radioTogglePressed() noexcept { return m_gamepad.radioTogglePressed(); }

    /* La croix haut ou bas vient-elle d'être pressée ? (balance radio/hélico, comme +/-) */
    [[nodiscard]] bool radioMixUpPressed() noexcept { return m_gamepad.radioMixUpPressed(); }
    [[nodiscard]] bool radioMixDownPressed() noexcept { return m_gamepad.radioMixDownPressed(); }

    /* Commande de regard du pilote (L3 tenu + stick droit X), manette uniquement :
       le clavier n'y participe pas. */
    [[nodiscard]] float lookAxis() const noexcept { return m_gamepad.lookAxis(); }

    /* L3 est-il tenu ? (regard du pilote en cours : l'angle reste où il est, même
       stick au neutre ; c'est le relâchement qui ramène la vue vers l'avant) */
    [[nodiscard]] bool lookHeld() const noexcept { return m_gamepad.lookHeld(); }

    /* Commande de regard vertical (L3 tenu + stick droit Y, haut = +1), manette
       uniquement. */
    [[nodiscard]] float lookAxisVertical() const noexcept { return m_gamepad.lookAxisVertical(); }

    [[nodiscard]] Source activeSource() const noexcept { return m_active; }

private:
    Keyboard m_keyboard;
    Gamepad  m_gamepad;
    Source   m_active = Source::Keyboard;
};

}  /* namespace artouste::input */
