/*
 * GamepadButtons.cpp
 * Détection de front des boutons de la manette (vue, turbine, HUD, pause,
 * reset, retour au menu, livrée, mode assisté, atterrissage automatique) et
 * des boutons maintenus (tir sur R3, regard du pilote sur L3). Complète
 * Gamepad.cpp (cycle de vie) et GamepadAxes.cpp (axes de vol).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "input/Gamepad.hpp"

#include <GLFW/glfw3.h>

namespace artouste::input {

namespace {

/* Front montant d'un bouton : vrai uniquement au passage de relâché à appuyé,
 * pour ne déclencher qu'une fois par appui. "prev" mémorise l'état précédent. */
bool risingEdge(const GLFWgamepadstate& state, int button, bool& prev) noexcept {
    const bool appuye = state.buttons[button] == GLFW_PRESS;
    const bool nouvelAppui = appuye && !prev;
    prev = appuye;
    return nouvelAppui;
}

} /* namespace */

bool Gamepad::viewTogglePressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevY = false;
        return false;
    }
    /* Bouton jaune Y de la manette Xbox (en haut du losange ABXY). */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_Y, m_prevY);
}

bool Gamepad::turbineTogglePressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevStart = false;
        return false;
    }
    /* Bouton Start (menu) de la manette Xbox : démarre ou coupe la turbine. */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_START, m_prevStart);
}

bool Gamepad::hudTogglePressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevB = false;
        return false;
    }
    /* Bouton B : affiche ou masque le HUD (comme la touche H). */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_B, m_prevB);
}

bool Gamepad::pauseTogglePressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevBack = false;
        return false;
    }
    /* Bouton Back (View) : met en pause ou reprend (comme la touche P). */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_BACK, m_prevBack);
}

bool Gamepad::resetPressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevX = false;
        return false;
    }
    /* Bouton X : replace l'appareil au point de départ (comme la touche R). */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_X, m_prevX);
}

bool Gamepad::menuPressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevMenu = false;
        return false;
    }
    /* Retour au menu : les deux gâchettes d'épaule (LB + RB) pressées ensemble. La
     * combinaison évite un retour sur un simple appui involontaire. On déclenche
     * une fois, au passage de "pas les deux" à "les deux" (front montant). */
    const bool both = state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS &&
                      state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
    const bool nouvelAppui = both && !m_prevMenu;
    m_prevMenu = both;
    return nouvelAppui;
}

bool Gamepad::liveryTogglePressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevA = false;
        return false;
    }
    /* Bouton A (vert, en bas du losange ABXY) : fait défiler la livrée. */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_A, m_prevA);
}

bool Gamepad::assistTogglePressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevLeftBumper = false;
        return false;
    }
    /* LB (L1 sur PS4/PS5) : bascule le mode assisté (comme la touche M). Ignorée si
       RB est aussi tenu : cette combinaison sert au retour au menu (menuPressed),
       pas au mode assisté -- sans quoi les deux se déclencheraient à la fois. On
       mémorise l'état brut de LB (pas "LB sans RB") pour ne pas redéclencher au
       relâchement de RB si LB était resté tenu pendant la combinaison. */
    const bool lb = state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS;
    const bool rb = state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
    const bool nouvelAppui = lb && !rb && !m_prevLeftBumper;
    m_prevLeftBumper = lb;
    return nouvelAppui;
}

bool Gamepad::autolandTogglePressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevRightBumper = false;
        return false;
    }
    /* RB (R1 sur PS4/PS5) : bascule l'atterrissage automatique (comme la touche J).
       Ignorée si LB est aussi tenu : cette combinaison sert au retour au menu
       (menuPressed), pas à l'atterrissage automatique -- sans quoi les deux se
       déclencheraient à la fois. Même logique de mémorisation que
       assistTogglePressed (état brut de RB, pas "RB sans LB"). */
    const bool rb = state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
    const bool lb = state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS;
    const bool nouvelAppui = rb && !lb && !m_prevRightBumper;
    m_prevRightBumper = rb;
    return nouvelAppui;
}

bool Gamepad::radioTogglePressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevDpadRight = false;
        return false;
    }
    /* Croix directionnelle droite : allume ou coupe la radio (comme la touche K). */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, m_prevDpadRight);
}

bool Gamepad::radioMixUpPressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevDpadUp = false;
        return false;
    }
    /* Croix directionnelle haut : balance vers la radio (comme la touche +). */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_DPAD_UP, m_prevDpadUp);
}

bool Gamepad::radioMixDownPressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevDpadDown = false;
        return false;
    }
    /* Croix directionnelle bas : balance vers l'hélico (comme la touche -). */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, m_prevDpadDown);
}

bool Gamepad::fireHeld() const noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        return false;
    }
    return state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] == GLFW_PRESS;
}

bool Gamepad::lookHeld() const noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        return false;
    }
    return state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] == GLFW_PRESS;
}

void Gamepad::primeButtons() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        /* Pas de manette : rien à tenir, tous les fronts repartent au repos. */
        m_prevY = m_prevStart = m_prevB = m_prevBack = m_prevX = m_prevMenu = m_prevA =
            m_prevLeftBumper = m_prevRightBumper = m_prevDpadRight = m_prevDpadUp =
                m_prevDpadDown = false;
        return;
    }
    const auto down = [&](int b) { return state.buttons[b] == GLFW_PRESS; };
    m_prevY = down(GLFW_GAMEPAD_BUTTON_Y);
    m_prevStart = down(GLFW_GAMEPAD_BUTTON_START);
    m_prevB = down(GLFW_GAMEPAD_BUTTON_B);
    m_prevBack = down(GLFW_GAMEPAD_BUTTON_BACK);
    m_prevX = down(GLFW_GAMEPAD_BUTTON_X);
    m_prevA = down(GLFW_GAMEPAD_BUTTON_A);
    m_prevLeftBumper = down(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER);
    m_prevRightBumper = down(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER);
    m_prevMenu = m_prevLeftBumper && m_prevRightBumper;
    /* La croix sert aussi à naviguer dans le menu : sans amorçage, la croix encore
       tenue en entrant en vol changerait la radio dès la première image. */
    m_prevDpadRight = down(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT);
    m_prevDpadUp = down(GLFW_GAMEPAD_BUTTON_DPAD_UP);
    m_prevDpadDown = down(GLFW_GAMEPAD_BUTTON_DPAD_DOWN);
}

} /* namespace artouste::input */
