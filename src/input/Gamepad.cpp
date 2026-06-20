/*
 * Gamepad.cpp
 * Conversion de l'état brut de la manette en commandes de vol.
 * Les sticks passent par une zone morte et une courbe d'expansion ; le
 * collectif est un levier piloté par les gâchettes (il garde sa position).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "input/Gamepad.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace artouste::input {

namespace {

constexpr int   PAD       = GLFW_JOYSTICK_1;  /* première manette */
constexpr float DEADZONE  = 0.07f;            /* zone morte ~7 % */
constexpr float EXPO      = 1.5f;             /* courbe d'expansion */

/* Seuil de "vraie sollicitation" pour décider que le joueur utilise la manette
 * (et non une simple dérive de stick ou une gâchette mal calibrée). Plus élevé
 * que la zone morte pour ne pas voler la priorité au clavier. */
constexpr float ACTIVE_THRESHOLD = 0.5f;

/* Vitesse du levier de collectif, en unités par seconde à pleine gâchette. */
constexpr float COLLECTIVE_RATE = 0.6f;

/* Applique zone morte puis expansion à un axe de stick brut [-1, +1].
 * La zone morte ignore les petits écarts ; l'expansion rend le centre
 * plus doux pour des corrections fines. */
float shapeAxis(float v) noexcept {
    const float a = std::fabs(v);
    if (a < DEADZONE) {
        return 0.0f;
    }
    const float normalized = (a - DEADZONE) / (1.0f - DEADZONE);
    const float curved      = std::pow(normalized, EXPO);
    return std::copysign(curved, v);
}

/* Convertit une gâchette GLFW (repos -1, fond +1) vers la plage [0, 1]. */
float triggerTo01(float v) noexcept {
    return (v + 1.0f) * 0.5f;
}

bool readState(GLFWgamepadstate& state) noexcept {
    return glfwJoystickIsGamepad(PAD) == GLFW_TRUE &&
           glfwGetGamepadState(PAD, &state) == GLFW_TRUE;
}

}  /* namespace */

bool Gamepad::isPresent() noexcept {
    return glfwJoystickIsGamepad(PAD) == GLFW_TRUE;
}

physics::Controls Gamepad::poll(float dt) noexcept {
    physics::Controls controls;
    controls.collective = m_collective;  /* on garde la position du levier */

    GLFWgamepadstate state;
    if (!readState(state)) {
        return controls;
    }

    const float* axes = state.axes;
    controls.cyclicLateral = shapeAxis(axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
    /* Stick vers le haut = Y négatif chez GLFW ; on inverse pour "avant = +1". */
    controls.cyclicLongitudinal = shapeAxis(-axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
    controls.pedals             = shapeAxis(axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);

    /* Collectif en levier : la gâchette droite (RT) fait monter le levier, la
     * gauche (LT) le fait descendre, à une vitesse proportionnelle à l'appui.
     * Au repos, le levier garde sa position (comme le vrai levier de collectif). */
    const float monte    = triggerTo01(axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);
    const float descend  = triggerTo01(axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]);
    const float commande = monte - descend;  /* dans [-1, +1] */
    if (std::fabs(commande) > 0.05f) {        /* on ignore le repos et le bruit */
        m_collective += commande * COLLECTIVE_RATE * dt;
        m_collective = std::clamp(m_collective, 0.0f, 1.0f);
    }
    controls.collective = m_collective;

    return controls;
}

namespace {

/* Front montant d'un bouton : vrai uniquement au passage de relâché à appuyé,
 * pour ne déclencher qu'une fois par appui. "prev" mémorise l'état précédent. */
bool risingEdge(const GLFWgamepadstate& state, int button, bool& prev) noexcept {
    const bool appuye      = state.buttons[button] == GLFW_PRESS;
    const bool nouvelAppui = appuye && !prev;
    prev                   = appuye;
    return nouvelAppui;
}

}  /* namespace */

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

bool Gamepad::quitPressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevQuit = false;
        return false;
    }
    /* Quitter : les deux gâchettes d'épaule (LB + RB) pressées ensemble. La
     * combinaison évite une sortie sur un simple appui involontaire. On déclenche
     * une fois, au passage de "pas les deux" à "les deux" (front montant). */
    const bool both = state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS &&
                      state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
    const bool nouvelAppui = both && !m_prevQuit;
    m_prevQuit             = both;
    return nouvelAppui;
}

bool Gamepad::liveryTogglePressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevA = false;
        return false;
    }
    /* Bouton A (vert, en bas du losange ABXY) : bascule la livrée Gendarmerie. */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_A, m_prevA);
}

bool Gamepad::assistTogglePressed() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        m_prevDpadUp = false;
        return false;
    }
    /* Croix directionnelle haut : bascule le mode assisté (comme la touche M). */
    return risingEdge(state, GLFW_GAMEPAD_BUTTON_DPAD_UP, m_prevDpadUp);
}

bool Gamepad::isActive() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        return false;
    }
    const float* axes = state.axes;
    /* Un stick nettement poussé (au-delà d'une simple dérive). */
    if (std::fabs(axes[GLFW_GAMEPAD_AXIS_LEFT_X]) > ACTIVE_THRESHOLD ||
        std::fabs(axes[GLFW_GAMEPAD_AXIS_LEFT_Y]) > ACTIVE_THRESHOLD ||
        std::fabs(axes[GLFW_GAMEPAD_AXIS_RIGHT_X]) > ACTIVE_THRESHOLD) {
        return true;
    }
    /* Une gâchette nettement enfoncée. Seuil élevé car certaines manettes
     * laissent la gâchette au repos à 0 (et non -1) : un seuil trop bas
     * verrouillerait la source sur la manette et ignorerait le clavier. */
    if (axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > ACTIVE_THRESHOLD ||
        axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > ACTIVE_THRESHOLD) {
        return true;
    }
    return false;
}

}  /* namespace artouste::input */
