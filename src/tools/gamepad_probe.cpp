// Outil de diagnostic manette (hors simulateur).
//
// Liste les manettes vues par GLFW, indique si elles sont reconnues comme
// "gamepad" (mapping SDL disponible, donc utilisable via glfwGetGamepadState),
// et affiche en direct axes et boutons. Les axes sont annotés selon le mapping
// de pilotage prévu pour vérifier les orientations.
//
// Usage : ./build/bin/gamepad_probe   (Ctrl+C pour quitter)
// Branchement à chaud pris en charge : on peut le lancer avant la manette.

#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

void errorCallback(int code, const char* description) {
    std::fprintf(stderr, "[GLFW] erreur %d : %s\n", code, description);
}

const char* boolText(int present) {
    return present == GLFW_TRUE ? "oui" : "non";
}

// Affiche l'état d'un gamepad reconnu : axes mappés + boutons enfoncés.
void printGamepad(int jid) {
    GLFWgamepadstate state;
    if (glfwGetGamepadState(jid, &state) != GLFW_TRUE) {
        std::printf("    (état gamepad indisponible)\n");
        return;
    }

    const float* a = state.axes;
    std::printf("    Cyclique  (stick G) : X %+.2f   Y %+.2f\n",
                static_cast<double>(a[GLFW_GAMEPAD_AXIS_LEFT_X]),
                static_cast<double>(a[GLFW_GAMEPAD_AXIS_LEFT_Y]));
    std::printf("    Palonnier (stick D) : X %+.2f\n",
                static_cast<double>(a[GLFW_GAMEPAD_AXIS_RIGHT_X]));
    std::printf("    Collectif (gâchettes): LT %+.2f   RT %+.2f   (repos = -1.00)\n",
                static_cast<double>(a[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]),
                static_cast<double>(a[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]));

    std::printf("    Boutons :");
    struct ButtonLabel {
        int         index;
        const char* name;
    };
    const ButtonLabel buttons[] = {
        {GLFW_GAMEPAD_BUTTON_A, "A"},
        {GLFW_GAMEPAD_BUTTON_B, "B"},
        {GLFW_GAMEPAD_BUTTON_X, "X"},
        {GLFW_GAMEPAD_BUTTON_Y, "Y"},
        {GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, "LB"},
        {GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, "RB"},
        {GLFW_GAMEPAD_BUTTON_BACK, "Back"},
        {GLFW_GAMEPAD_BUTTON_START, "Start"},
        {GLFW_GAMEPAD_BUTTON_GUIDE, "Guide"},
        {GLFW_GAMEPAD_BUTTON_LEFT_THUMB, "L3"},
        {GLFW_GAMEPAD_BUTTON_RIGHT_THUMB, "R3"},
        {GLFW_GAMEPAD_BUTTON_DPAD_UP, "Haut"},
        {GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, "Droite"},
        {GLFW_GAMEPAD_BUTTON_DPAD_DOWN, "Bas"},
        {GLFW_GAMEPAD_BUTTON_DPAD_LEFT, "Gauche"},
    };
    bool any = false;
    for (const ButtonLabel& b : buttons) {
        if (state.buttons[b.index] == GLFW_PRESS) {
            std::printf(" %s", b.name);
            any = true;
        }
    }
    std::printf("%s\n", any ? "" : " (aucun)");
}

}  // namespace

int main() {
    glfwSetErrorCallback(errorCallback);
    if (glfwInit() != GLFW_TRUE) {
        std::fprintf(stderr, "Impossible d'initialiser GLFW.\n");
        return EXIT_FAILURE;
    }

    std::printf("Sonde manette - GLFW %s\n", glfwGetVersionString());
    std::printf("Branche ta manette Xbox ; Ctrl+C pour quitter.\n");

    while (true) {
        glfwPollEvents();  // nécessaire au branchement à chaud

        std::printf("\033[H\033[J");  // efface l'écran, curseur en haut
        std::printf("=== Sonde manette (Ctrl+C pour quitter) ===\n\n");

        int found = 0;
        for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
            if (glfwJoystickPresent(jid) != GLFW_TRUE) {
                continue;
            }
            ++found;

            const char* joyName     = glfwGetJoystickName(jid);
            const int   isGamepad   = glfwJoystickIsGamepad(jid);
            const char* gamepadName = isGamepad == GLFW_TRUE ? glfwGetGamepadName(jid) : "-";

            std::printf("[%d] %s\n", jid, joyName != nullptr ? joyName : "(sans nom)");
            std::printf("    reconnu gamepad : %s", boolText(isGamepad));
            if (isGamepad == GLFW_TRUE) {
                std::printf("   (%s)\n", gamepadName != nullptr ? gamepadName : "?");
                printGamepad(jid);
            } else {
                std::printf("\n    -> pas de mapping SDL : glfwGetGamepadState indisponible.\n");
            }
            std::printf("\n");
        }

        if (found == 0) {
            std::printf("Aucune manette détectée. Branche-la, elle apparaîtra ici.\n");
        }
        std::fflush(stdout);

        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }

    // Inaccessible (sortie par Ctrl+C) ; présent pour la symétrie.
    glfwTerminate();
    return EXIT_SUCCESS;
}
