/*
 * ApplicationMenuEntrees.cpp
 * Lecture des entrées du menu (voir ApplicationMenuEntrees.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/menu/ApplicationMenuEntrees.hpp"

#include "input/Keyboard.hpp"

#include <GLFW/glfw3.h>

namespace artouste::app {

EntreesMenu lireEntreesMenu(GLFWwindow* fenetre) {
    EntreesMenu e;
    e.haut    = glfwGetKey(fenetre, GLFW_KEY_UP) == GLFW_PRESS;
    e.bas     = glfwGetKey(fenetre, GLFW_KEY_DOWN) == GLFW_PRESS;
    e.valider = glfwGetKey(fenetre, GLFW_KEY_ENTER) == GLFW_PRESS ||
                glfwGetKey(fenetre, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    e.turbine = glfwGetKey(fenetre, GLFW_KEY_SPACE) == GLFW_PRESS;
    /* Lettre résolue par la disposition réelle : le raccourci suit la lettre
       imprimée sur la touche. */
    e.demo    = glfwGetKey(fenetre, input::toucheImprimant('d')) == GLFW_PRESS;
    e.quitter = glfwGetKey(fenetre, GLFW_KEY_ESCAPE) == GLFW_PRESS;

    /* Première manette reconnue, quel que soit son slot : un périphérique
       virtuel peut occuper le slot 0 et reléguer la vraie manette plus loin. */
    int jid = -1;
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; ++j) {
        if (glfwJoystickIsGamepad(j) == GLFW_TRUE) {
            jid = j;
            break;
        }
    }
    GLFWgamepadstate gp;
    if (jid >= 0 && glfwGetGamepadState(jid, &gp) == GLFW_TRUE) {
        e.haut = e.haut || gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
                 gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -0.5f;
        e.bas = e.bas || gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
                gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > 0.5f;
        e.valider = e.valider || gp.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
        e.turbine = e.turbine || gp.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
        e.demo    = e.demo || gp.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS;
        e.quitter = e.quitter || gp.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
    }
    return e;
}

} /* namespace artouste::app */
