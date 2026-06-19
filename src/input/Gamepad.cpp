/*
 * Gamepad.cpp
 * Conversion de l'état brut de la manette en commandes de vol.
 * Les sticks passent par une zone morte et une courbe d'expansion ;
 * la gâchette droite est convertie en collectif.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "input/Gamepad.hpp"

#include <GLFW/glfw3.h>

#include <cmath>

namespace artouste::input {

namespace {

constexpr int   PAD       = GLFW_JOYSTICK_1;  /* première manette */
constexpr float DEADZONE  = 0.07f;            /* zone morte ~7 % */
constexpr float EXPO      = 1.5f;             /* courbe d'expansion */

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

physics::Controls Gamepad::poll() noexcept {
    physics::Controls controls;

    GLFWgamepadstate state;
    if (!readState(state)) {
        return controls;
    }

    const float* axes = state.axes;
    controls.cyclicLateral = shapeAxis(axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
    /* Stick vers le haut = Y négatif chez GLFW ; on inverse pour "avant = +1". */
    controls.cyclicLongitudinal = shapeAxis(-axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
    controls.pedals             = shapeAxis(axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);

    /* Collectif proportionnel à la gâchette droite : relâchée = 0 (posé au sol),
     * à fond = collectif plein. La gâchette gauche n'est pas utilisée. */
    controls.collective = triggerTo01(axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);

    return controls;
}

bool Gamepad::isActive() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        return false;
    }
    const float* axes = state.axes;
    if (std::fabs(axes[GLFW_GAMEPAD_AXIS_LEFT_X]) > DEADZONE ||
        std::fabs(axes[GLFW_GAMEPAD_AXIS_LEFT_Y]) > DEADZONE ||
        std::fabs(axes[GLFW_GAMEPAD_AXIS_RIGHT_X]) > DEADZONE) {
        return true;
    }
    /* Gâchettes nettement enfoncées (bien au-delà du repos -1). */
    if (axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > -0.8f ||
        axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > -0.8f) {
        return true;
    }
    return false;
}

}  /* namespace artouste::input */
