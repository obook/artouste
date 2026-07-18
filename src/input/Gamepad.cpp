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
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace artouste::input {

namespace {

constexpr float DEADZONE  = 0.07f;            /* zone morte ~7 % */
constexpr float EXPO      = 1.5f;             /* courbe d'expansion */

/* Seuil de "vraie sollicitation" pour décider que le joueur utilise la manette
 * (et non une simple dérive de stick ou une gâchette mal calibrée). Plus élevé
 * que la zone morte pour ne pas voler la priorité au clavier. */
constexpr float ACTIVE_THRESHOLD = 0.5f;

/* Vitesse du levier de collectif, en unités par seconde à pleine gâchette. */
constexpr float COLLECTIVE_RATE = 0.6f;

/* Drapeau levé par le callback joystick de GLFW à la déconnexion d'une manette,
 * consommé au prochain poll pour remettre le levier de collectif à zéro. Le
 * callback et poll s'exécutent sur le même thread (glfwPollEvents) : pas de
 * concurrence à gérer. */
bool g_collectiveResetRequested = false;

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

/* Renvoie le premier slot GLFW occupé par une manette reconnue (mapping SDL
 * disponible), ou -1 si aucune. On ne se limite pas au slot 0 : un périphérique
 * virtuel (greffon de streaming, pédalier, etc.) peut occuper ce slot et
 * reléguer la vraie manette sur un slot suivant. */
int activePad() noexcept {
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickIsGamepad(jid) == GLFW_TRUE) {
            return jid;
        }
    }
    return -1;
}

bool readState(GLFWgamepadstate& state) noexcept {
    const int jid = activePad();
    return jid >= 0 && glfwGetGamepadState(jid, &state) == GLFW_TRUE;
}

}  /* namespace */

void Gamepad::loadMappings(const std::filesystem::path& assetDir) noexcept {
    const std::filesystem::path fichier = assetDir / "gamecontrollerdb.txt";
    std::ifstream in(fichier, std::ios::binary);
    if (!in) {
        /* Fichier absent : ce n'est pas une erreur. GLFW garde sa base intégrée,
         * qui reconnaît déjà la plupart des manettes courantes. */
        return;
    }
    const std::string contenu((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    if (contenu.empty()) {
        return;
    }
    /* glfwUpdateGamepadMappings accepte le fichier entier (une ligne par manette) ;
     * les mappings du fichier complètent ou remplacent ceux de la base intégrée. */
    if (glfwUpdateGamepadMappings(contenu.c_str()) != GLFW_TRUE) {
        std::fprintf(stderr,
                     "Artouste : base de mappings manette non chargée (%s).\n",
                     fichier.string().c_str());
    }
}

bool Gamepad::isPresent() noexcept {
    return activePad() >= 0;
}

void Gamepad::onJoystickEvent(int /*jid*/, int event) noexcept {
    if (event == GLFW_DISCONNECTED) {
        g_collectiveResetRequested = true;
    }
}

physics::Controls Gamepad::poll(float dt) noexcept {
    /* Une manette a été débranchée depuis le dernier poll : on remet le levier
     * de collectif à zéro avant de reprendre la lecture. */
    if (g_collectiveResetRequested) {
        g_collectiveResetRequested = false;
        m_collective = 0.0f;
    }

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
    m_prevMenu             = both;
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
    const bool lb           = state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS;
    const bool rb           = state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
    const bool nouvelAppui  = lb && !rb && !m_prevLeftBumper;
    m_prevLeftBumper        = lb;
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
    const bool rb           = state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
    const bool lb           = state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS;
    const bool nouvelAppui  = rb && !lb && !m_prevRightBumper;
    m_prevRightBumper       = rb;
    return nouvelAppui;
}

bool Gamepad::fireHeld() const noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        return false;
    }
    return state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] == GLFW_PRESS;
}

void Gamepad::primeButtons() noexcept {
    GLFWgamepadstate state;
    if (!readState(state)) {
        /* Pas de manette : rien à tenir, tous les fronts repartent au repos. */
        m_prevY = m_prevStart = m_prevB = m_prevBack = m_prevX = m_prevMenu = m_prevA =
            m_prevLeftBumper = m_prevRightBumper = false;
        return;
    }
    const auto down = [&](int b) { return state.buttons[b] == GLFW_PRESS; };
    m_prevY           = down(GLFW_GAMEPAD_BUTTON_Y);
    m_prevStart       = down(GLFW_GAMEPAD_BUTTON_START);
    m_prevB           = down(GLFW_GAMEPAD_BUTTON_B);
    m_prevBack        = down(GLFW_GAMEPAD_BUTTON_BACK);
    m_prevX           = down(GLFW_GAMEPAD_BUTTON_X);
    m_prevA           = down(GLFW_GAMEPAD_BUTTON_A);
    m_prevLeftBumper  = down(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER);
    m_prevRightBumper = down(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER);
    m_prevMenu        = m_prevLeftBumper && m_prevRightBumper;
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
