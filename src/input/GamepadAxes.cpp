/*
 * GamepadAxes.cpp
 * Conversion des axes bruts de la manette (sticks et gâchettes) en commandes
 * de vol. Les sticks passent par une zone morte et une courbe d'expansion ;
 * le collectif est un levier piloté par les gâchettes (il garde sa position).
 * Complète Gamepad.cpp (cycle de vie) et GamepadButtons.cpp (boutons).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "input/Gamepad.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace artouste::input {

namespace {

constexpr float DEADZONE = 0.07f; /* zone morte ~7 % */
constexpr float EXPO = 1.5f;      /* courbe d'expansion */

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
    const float curved = std::pow(normalized, EXPO);
    return std::copysign(curved, v);
}

/* Convertit une gâchette GLFW (repos -1, fond +1) vers la plage [0, 1]. */
float triggerTo01(float v) noexcept {
    return (v + 1.0f) * 0.5f;
}

} /* namespace */

physics::Controls Gamepad::poll(float dt) noexcept {
    /* Une manette a été débranchée depuis le dernier poll : on remet le levier
     * de collectif à zéro avant de reprendre la lecture. */
    if (s_collectiveResetRequested) {
        s_collectiveResetRequested = false;
        m_collective = 0.0f;
    }

    physics::Controls controls;
    controls.collective = m_collective; /* on garde la position du levier */

    GLFWgamepadstate state;
    if (!readState(state)) {
        return controls;
    }

    const float* axes = state.axes;
    controls.cyclicLateral = shapeAxis(axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
    /* Stick vers le haut = Y négatif chez GLFW ; on inverse pour "avant = +1". */
    controls.cyclicLongitudinal = shapeAxis(-axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
    controls.pedals = shapeAxis(axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);

    /* Collectif en levier : la gâchette droite (RT) fait monter le levier, la
     * gauche (LT) le fait descendre, à une vitesse proportionnelle à l'appui.
     * Au repos, le levier garde sa position (comme le vrai levier de collectif). */
    const float monte = triggerTo01(axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);
    const float descend = triggerTo01(axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]);
    const float commande = monte - descend; /* dans [-1, +1] */
    if (std::fabs(commande) > 0.05f) {      /* on ignore le repos et le bruit */
        m_collective += commande * COLLECTIVE_RATE * dt;
        m_collective = std::clamp(m_collective, 0.0f, 1.0f);
    }
    controls.collective = m_collective;

    return controls;
}

} /* namespace artouste::input */
