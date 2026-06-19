#include "input/Keyboard.hpp"

#include <GLFW/glfw3.h>

#include "util/Math.hpp"

namespace artouste::input {

namespace {

constexpr float COLLECTIVE_RATE = 0.6f;  // variation par seconde, touche maintenue
constexpr float RECENTER_RATE   = 4.0f;  // retour au neutre des axes à ressort
constexpr float COMMAND_RATE    = 6.0f;  // montée en commande des axes à ressort

bool down(GLFWwindow* window, int key) noexcept {
    return glfwGetKey(window, key) == GLFW_PRESS;
}

// Fait tendre une commande à ressort vers sa cible (-1, 0 ou +1) puis se
// recentre quand la cible est nulle.
float spring(float current, float target, float dt) noexcept {
    const float rate = (target == 0.0f) ? RECENTER_RATE : COMMAND_RATE;
    return lerp(current, target, saturate(rate * dt));
}

}  // namespace

physics::Controls Keyboard::poll(float dt) noexcept {
    if (m_window == nullptr) {
        return m_controls;
    }

    // Collectif : maintenu. W ou Z montent, S descend (W et A/Q tolèrent AZERTY).
    float collectiveDelta = 0.0f;
    if (down(m_window, GLFW_KEY_W) || down(m_window, GLFW_KEY_Z)) {
        collectiveDelta += COLLECTIVE_RATE * dt;
    }
    if (down(m_window, GLFW_KEY_S)) {
        collectiveDelta -= COLLECTIVE_RATE * dt;
    }
    m_controls.collective = saturate(m_controls.collective + collectiveDelta);

    // Cyclique longitudinal : flèche haut = avant (+1), bas = arrière (-1).
    float pitchTarget = 0.0f;
    if (down(m_window, GLFW_KEY_UP)) {
        pitchTarget += 1.0f;
    }
    if (down(m_window, GLFW_KEY_DOWN)) {
        pitchTarget -= 1.0f;
    }
    m_controls.cyclicLongitudinal = spring(m_controls.cyclicLongitudinal, pitchTarget, dt);

    // Cyclique latéral : flèche droite = +1, gauche = -1.
    float rollTarget = 0.0f;
    if (down(m_window, GLFW_KEY_RIGHT)) {
        rollTarget += 1.0f;
    }
    if (down(m_window, GLFW_KEY_LEFT)) {
        rollTarget -= 1.0f;
    }
    m_controls.cyclicLateral = spring(m_controls.cyclicLateral, rollTarget, dt);

    // Palonniers : D = droite (+1), A ou Q = gauche (-1).
    float yawTarget = 0.0f;
    if (down(m_window, GLFW_KEY_D)) {
        yawTarget += 1.0f;
    }
    if (down(m_window, GLFW_KEY_A) || down(m_window, GLFW_KEY_Q)) {
        yawTarget -= 1.0f;
    }
    m_controls.pedals = spring(m_controls.pedals, yawTarget, dt);

    return m_controls;
}

bool Keyboard::isActive() const noexcept {
    if (m_window == nullptr) {
        return false;
    }
    const int keys[] = {GLFW_KEY_W,    GLFW_KEY_Z,  GLFW_KEY_S,     GLFW_KEY_UP,
                        GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_A,
                        GLFW_KEY_Q,    GLFW_KEY_D};
    for (const int key : keys) {
        if (down(m_window, key)) {
            return true;
        }
    }
    return false;
}

}  // namespace artouste::input
