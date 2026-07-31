/*
 * Keyboard.cpp
 * Conversion des touches enfoncées en commandes de vol.
 * Le collectif se règle par paliers maintenus. Le cyclique (flèches) est à
 * rémanence, comme un vrai manche : une flèche maintenue le déplace vers
 * l'extrême correspondant, et il garde la position atteinte au relâchement
 * (pas de retour automatique au neutre, qui empêchait de tenir une petite
 * inclinaison stable sans garder la touche enfoncée). Espace le recentre
 * franchement. Les palonniers, eux, restent à ressort : au repos, un appareil
 * qui part en lacet est plus gênant qu'un cyclique légèrement décentré.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "input/Keyboard.hpp"

#include <GLFW/glfw3.h>

#include "util/Math.hpp"

#include <array>
#include <cstddef>

namespace artouste::input {

int toucheImprimant(char lettre) {
    /* Table dressée une fois : glfwGetKeyName interroge la disposition du
       système, ce qui n'a pas à être refait à chaque image. Le balayage couvre
       les touches imprimables, et pas seulement A-Z : sur AZERTY, la lettre M
       occupe la position que GLFW nomme GLFW_KEY_SEMICOLON. */
    static const auto table = [] {
        std::array<int, 26> jetons{};
        for (std::size_t i = 0; i < jetons.size(); ++i) {
            jetons[i] = GLFW_KEY_A + static_cast<int>(i);  /* repli : disposition US */
        }
        for (int jeton = GLFW_KEY_SPACE; jeton <= GLFW_KEY_GRAVE_ACCENT; ++jeton) {
            /* La disposition du système n'a pas forcément une touche physique à
               cette position : sur un clavier français, plusieurs positions que
               GLFW nomme d'après le clavier américain n'existent pas. GLFW rend
               alors un scancode de -1, et glfwGetKeyName le passe tel quel à
               glfwGetScancodeName, qui signale GLFW_INVALID_VALUE. On écartait
               bien le résultat (nom nul), mais après coup : le journal se
               remplissait d'une salve de "Invalid scancode -1" au démarrage,
               onze sur un AZERTY. Tester le scancode d'abord donne exactement
               le même tableau, sans le bruit. */
            if (glfwGetKeyScancode(jeton) < 0) {
                continue;
            }
            const char* nom = glfwGetKeyName(jeton, 0);
            if (nom == nullptr || nom[0] < 'a' || nom[0] > 'z' || nom[1] != '\0') {
                continue;
            }
            jetons[static_cast<std::size_t>(nom[0] - 'a')] = jeton;
        }
        return jetons;
    }();

    if (lettre < 'a' || lettre > 'z') {
        return GLFW_KEY_UNKNOWN;
    }
    return table[static_cast<std::size_t>(lettre - 'a')];
}

namespace {

constexpr float COLLECTIVE_RATE = 0.6f;  /* variation par seconde, touche maintenue */
constexpr float RECENTER_RATE   = 4.0f;  /* retour au neutre (palonniers, et cyclique sur Espace) */
constexpr float COMMAND_RATE    = 6.0f;  /* montée en commande vers l'extrême, touche maintenue */

bool down(GLFWwindow* window, int key) noexcept {
    return glfwGetKey(window, key) == GLFW_PRESS;
}

/* Fait tendre une commande à ressort vers sa cible (-1, 0 ou +1), et la
 * ramène au neutre quand la cible est nulle. Sert aux palonniers. */
float spring(float current, float target, float dt) noexcept {
    const float rate = (target == 0.0f) ? RECENTER_RATE : COMMAND_RATE;
    return lerp(current, target, saturate(rate * dt));
}

/* Fait tendre un axe à rémanence vers sa cible, seulement si "active" (une
 * touche le sollicite) ; sinon la valeur courante est conservée telle quelle,
 * pour que le cyclique reste où on l'a laissé. */
float sticky(float current, float target, bool active, float rate, float dt) noexcept {
    return active ? lerp(current, target, saturate(rate * dt)) : current;
}

}  /* namespace */

physics::Controls Keyboard::poll(float dt) noexcept {
    if (m_window == nullptr) {
        return m_controls;
    }

    /* Collectif : position maintenue. Z monte, S descend.
       AZERTY : la touche marquée "Z" occupe la position physique du "W" QWERTY,
       que GLFW nomme GLFW_KEY_W ; on accepte les deux pour que le "Z" imprimé
       réponde aussi bien sur AZERTY que sur QWERTY. La touche S est au même
       endroit sur les deux dispositions. */
    float collectiveDelta = 0.0f;
    if (down(m_window, GLFW_KEY_Z) || down(m_window, GLFW_KEY_W)) {
        collectiveDelta += COLLECTIVE_RATE * dt;
    }
    if (down(m_window, GLFW_KEY_S)) {
        collectiveDelta -= COLLECTIVE_RATE * dt;
    }
    m_controls.collective = saturate(m_controls.collective + collectiveDelta);

    /* Cyclique, à rémanence : flèche maintenue = déplacement vers l'extrême
       correspondant (avant/arrière, droite/gauche) ; relâchée, la position
       atteinte est conservée. Espace force le retour au neutre sur les deux
       axes à la fois, comme on lâcherait franchement le manche. */
    const bool  recenter     = down(m_window, GLFW_KEY_SPACE);
    const float cyclicRate   = recenter ? RECENTER_RATE : COMMAND_RATE;

    /* Longitudinal : flèche haut = avant (+1), bas = arrière (-1). */
    float pitchTarget = 0.0f;
    bool  pitchActive = recenter;
    if (down(m_window, GLFW_KEY_UP)) {
        pitchTarget += 1.0f;
        pitchActive = true;
    }
    if (down(m_window, GLFW_KEY_DOWN)) {
        pitchTarget -= 1.0f;
        pitchActive = true;
    }
    if (recenter) {
        pitchTarget = 0.0f;  /* Espace l'emporte sur une flèche tenue par erreur */
    }
    m_controls.cyclicLongitudinal =
        sticky(m_controls.cyclicLongitudinal, pitchTarget, pitchActive, cyclicRate, dt);

    /* Latéral : flèche droite = +1, gauche = -1. */
    float rollTarget = 0.0f;
    bool  rollActive = recenter;
    if (down(m_window, GLFW_KEY_RIGHT)) {
        rollTarget += 1.0f;
        rollActive = true;
    }
    if (down(m_window, GLFW_KEY_LEFT)) {
        rollTarget -= 1.0f;
        rollActive = true;
    }
    if (recenter) {
        rollTarget = 0.0f;
    }
    m_controls.cyclicLateral = sticky(m_controls.cyclicLateral, rollTarget, rollActive, cyclicRate, dt);

    /* Palonniers : D = droite (+1), Q = gauche (-1). AZERTY : la touche marquée
       "Q" occupe la position physique du "A" QWERTY (GLFW_KEY_A) ; on accepte les
       deux pour couvrir AZERTY et QWERTY. D est identique sur les deux dispositions. */
    float yawTarget = 0.0f;
    if (down(m_window, GLFW_KEY_D)) {
        yawTarget += 1.0f;
    }
    if (down(m_window, GLFW_KEY_Q) || down(m_window, GLFW_KEY_A)) {
        yawTarget -= 1.0f;
    }
    m_controls.pedals = spring(m_controls.pedals, yawTarget, dt);

    return m_controls;
}

bool Keyboard::fireHeld() const noexcept {
    return m_window != nullptr && down(m_window, GLFW_KEY_LEFT_CONTROL);
}

bool Keyboard::isActive() const noexcept {
    if (m_window == nullptr) {
        return false;
    }
    /* Liste de toutes les touches de pilotage à surveiller. */
    const int keys[] = {GLFW_KEY_Z,     GLFW_KEY_W,     GLFW_KEY_S,
                        GLFW_KEY_UP,    GLFW_KEY_DOWN,  GLFW_KEY_LEFT,  GLFW_KEY_RIGHT,
                        GLFW_KEY_Q,     GLFW_KEY_A,     GLFW_KEY_D,     GLFW_KEY_SPACE};
    for (const int key : keys) {
        if (down(m_window, key)) {
            return true;
        }
    }
    return false;
}

}  /* namespace artouste::input */
