/*
 * Gamepad.cpp
 * Cycle de vie de la manette : chargement des mappings SDL, détection de
 * présence et de connexion/déconnexion, détection d'activité (choix de la
 * source de commande). Les axes de vol vivent dans GamepadAxes.cpp, la
 * détection de front des boutons dans GamepadButtons.cpp ; les trois fichiers
 * partagent les méthodes privées activePad()/readState() et le drapeau
 * s_collectiveResetRequested, définis ici.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "input/Gamepad.hpp"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace artouste::input {

namespace {

/* Seuil de "vraie sollicitation" pour décider que le joueur utilise la manette
 * (et non une simple dérive de stick ou une gâchette mal calibrée). Plus élevé
 * que la zone morte pour ne pas voler la priorité au clavier. */
constexpr float ACTIVE_THRESHOLD = 0.5f;

} /* namespace */

bool Gamepad::s_collectiveResetRequested = false;

int Gamepad::activePad() noexcept {
    /* Renvoie le premier slot GLFW occupé par une manette reconnue (mapping SDL
     * disponible), ou -1 si aucune. On ne se limite pas au slot 0 : un périphérique
     * virtuel (greffon de streaming, pédalier, etc.) peut occuper ce slot et
     * reléguer la vraie manette sur un slot suivant. */
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickIsGamepad(jid) == GLFW_TRUE) {
            return jid;
        }
    }
    return -1;
}

bool Gamepad::readState(GLFWgamepadstate& state) noexcept {
    const int jid = activePad();
    return jid >= 0 && glfwGetGamepadState(jid, &state) == GLFW_TRUE;
}

void Gamepad::loadMappings(const std::filesystem::path& assetDir) noexcept {
    /* L'ordre compte : le second fichier redéfinit les GUID déjà vus dans le
     * premier. Les correctifs maison passent donc après la base communautaire,
     * qui reste ainsi remplaçable telle quelle par sa dernière version. */
    chargerFichierMappings(assetDir / "gamecontrollerdb.txt");
    chargerFichierMappings(assetDir / "gamecontrollerdb-extra.txt");
}

void Gamepad::chargerFichierMappings(const std::filesystem::path& fichier) noexcept {
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
        s_collectiveResetRequested = true;
    }
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

} /* namespace artouste::input */
