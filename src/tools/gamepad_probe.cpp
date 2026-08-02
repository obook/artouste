/*
 * gamepad_probe.cpp
 * Petit outil de diagnostic de manette, indépendant du simulateur. Il liste
 * les manettes vues par GLFW, affiche leur GUID SDL, indique si chacune est
 * reconnue comme "gamepad" (un mapping SDL existe, donc glfwGetGamepadState
 * est utilisable), et affiche en direct ses axes et ses boutons. Les axes sont
 * annotés selon le pilotage prévu, ce qui permet de vérifier leurs orientations.
 *
 * L'état brut (axes a0..aN, boutons b0..bN, chapeau h0) est affiché dans tous
 * les cas, sous l'état mappé. Le GUID et ces numéros bruts sont exactement ce
 * qu'il faut pour écrire une ligne dans assets/gamecontrollerdb-extra.txt, et
 * les garder sous les yeux permet de voir tout de suite ce qu'une commande
 * produit vraiment quand le mappage semble faux.
 *
 * Usage : ./build/bin/gamepad_probe   (Ctrl+C pour quitter)
 * Le branchement à chaud est géré : on peut le lancer avant la manette.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

namespace {

/* Charge un fichier de mappings SDL s'il existe, cherché via ARTOUSTE_ASSETS
 * puis dans "assets" à côté du répertoire courant. Silencieux s'il est
 * introuvable. */
void chargerFichierMappings(const char* nom) {
    std::string chemin = std::string("assets/") + nom;
    if (const char* env = std::getenv("ARTOUSTE_ASSETS")) {
        chemin = std::string(env) + "/" + nom;
    }
    std::ifstream in(chemin, std::ios::binary);
    if (!in) {
        return;
    }
    const std::string contenu((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    if (!contenu.empty()) {
        glfwUpdateGamepadMappings(contenu.c_str());
    }
}

/* Charge les mêmes bases de mappings que le simulateur, dans le même ordre,
 * pour que la sonde reconnaisse exactement les mêmes manettes que lui. */
void chargerMappings() {
    chargerFichierMappings("gamecontrollerdb.txt");
    chargerFichierMappings("gamecontrollerdb-extra.txt");
}

void errorCallback(int code, const char* description) {
    std::fprintf(stderr, "[GLFW] erreur %d : %s\n", code, description);
}

const char* boolText(int present) {
    return present == GLFW_TRUE ? "oui" : "non";
}

/* Affiche l'état d'un gamepad reconnu : ses axes mappés et les boutons enfoncés. */
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

/* Bornes maximales atteintes par les axes bruts. La valeur au repos ne dit rien :
 * un axe qui n'existe que sur le papier reste à zéro comme un axe centré. Ce
 * relevé cumulé distingue les deux, et évite d'avoir à lire les valeurs au vol
 * en actionnant la commande d'une main. */
constexpr int MAX_JOYSTICKS = GLFW_JOYSTICK_LAST + 1;
constexpr int MAX_AXES      = 16;
constexpr int MAX_BUTTONS   = 32;

struct HistoriqueBrut {
    float        axisMin[MAX_AXES]     = {0.0f};
    float        axisMax[MAX_AXES]     = {0.0f};
    bool         buttonSeen[MAX_BUTTONS] = {false};
    unsigned int hatSeen                = 0;
};

HistoriqueBrut g_historique[MAX_JOYSTICKS];

/* Affiche l'état brut d'une manette : axes, boutons et chapeaux numérotés comme
 * dans la syntaxe des mappings (a0, b0, h0), puis le relevé cumulé depuis le
 * lancement. Les boutons ne sont listés que lorsqu'ils sont enfoncés, pour
 * pouvoir relever la numérotation en appuyant dessus un par un ; le relevé, lui,
 * garde tout ce qui a bougé au moins une fois. */
void printRaw(int jid) {
    HistoriqueBrut& hist = g_historique[jid];

    int          axisCount = 0;
    const float* axes      = glfwGetJoystickAxes(jid, &axisCount);
    if (axisCount > MAX_AXES) {
        axisCount = MAX_AXES;
    }
    std::printf("    Axes bruts (%d) :", axisCount);
    for (int i = 0; i < axisCount; ++i) {
        hist.axisMin[i] = std::min(hist.axisMin[i], axes[i]);
        hist.axisMax[i] = std::max(hist.axisMax[i], axes[i]);
        std::printf("  a%d %+.2f", i, static_cast<double>(axes[i]));
    }
    std::printf("%s\n", axisCount == 0 ? " (aucun)" : "");

    int                  buttonCount = 0;
    const unsigned char* buttons     = glfwGetJoystickButtons(jid, &buttonCount);
    if (buttonCount > MAX_BUTTONS) {
        buttonCount = MAX_BUTTONS;
    }
    std::printf("    Boutons bruts (%d) enfoncés :", buttonCount);
    bool anyButton = false;
    for (int i = 0; i < buttonCount; ++i) {
        if (buttons[i] == GLFW_PRESS) {
            hist.buttonSeen[i] = true;
            std::printf(" b%d", i);
            anyButton = true;
        }
    }
    std::printf("%s\n", anyButton ? "" : " (aucun)");

    int                  hatCount = 0;
    const unsigned char* hats     = glfwGetJoystickHats(jid, &hatCount);
    std::printf("    Chapeaux bruts (%d) :", hatCount);
    for (int i = 0; i < hatCount; ++i) {
        hist.hatSeen |= hats[i];
        std::printf("  h%d 0x%x", i, hats[i]);
    }
    std::printf("%s\n", hatCount == 0 ? " (aucun)" : "");

    /* Relevé cumulé : c'est lui qui tranche. Un axe resté à une amplitude nulle
     * n'est jamais piloté par aucune commande, quoi qu'en dise le pilote. */
    std::printf("    Relevé depuis le lancement (bouger tout, appuyer partout) :\n");
    for (int i = 0; i < axisCount; ++i) {
        const float amplitude = hist.axisMax[i] - hist.axisMin[i];
        std::printf("      a%d  de %+.2f à %+.2f   amplitude %.2f%s\n", i,
                    static_cast<double>(hist.axisMin[i]),
                    static_cast<double>(hist.axisMax[i]),
                    static_cast<double>(amplitude),
                    amplitude < 0.10f ? "   <-- INERTE" : "");
    }
    std::printf("      boutons vus :");
    bool anySeen = false;
    for (int i = 0; i < buttonCount; ++i) {
        if (hist.buttonSeen[i]) {
            std::printf(" b%d", i);
            anySeen = true;
        }
    }
    std::printf("%s\n", anySeen ? "" : " (aucun)");
    std::printf("      directions de chapeau vues : 0x%x\n", hist.hatSeen);
}

}  /* namespace */

int main() {
    glfwSetErrorCallback(errorCallback);
    if (glfwInit() != GLFW_TRUE) {
        std::fprintf(stderr, "Impossible d'initialiser GLFW.\n");
        return EXIT_FAILURE;
    }

    chargerMappings();

    std::printf("Sonde manette - GLFW %s\n", glfwGetVersionString());
    std::printf("Branche ta manette Xbox ; Ctrl+C pour quitter.\n");

    while (true) {
        glfwPollEvents();  /* nécessaire pour détecter le branchement à chaud */

        std::printf("\033[H\033[J");  /* efface l'écran et replace le curseur en haut */
        std::printf("=== Sonde manette (Ctrl+C pour quitter) ===\n\n");

        int found = 0;
        for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
            if (glfwJoystickPresent(jid) != GLFW_TRUE) {
                continue;
            }
            ++found;

            const char* joyName     = glfwGetJoystickName(jid);
            const char* guid        = glfwGetJoystickGUID(jid);
            const int   isGamepad   = glfwJoystickIsGamepad(jid);
            const char* gamepadName = isGamepad == GLFW_TRUE ? glfwGetGamepadName(jid) : "-";

            std::printf("[%d] %s\n", jid, joyName != nullptr ? joyName : "(sans nom)");
            std::printf("    GUID SDL : %s\n", guid != nullptr ? guid : "(inconnu)");
            std::printf("    reconnu gamepad : %s", boolText(isGamepad));
            if (isGamepad == GLFW_TRUE) {
                std::printf("   (%s)\n", gamepadName != nullptr ? gamepadName : "?");
                printGamepad(jid);
            } else {
                std::printf("\n    -> pas de mapping SDL pour ce GUID.\n");
                std::printf("       Pour la faire reconnaître, ajouter une ligne à\n");
                std::printf("       assets/gamecontrollerdb-extra.txt (voir son entête).\n");
            }
            /* Toujours affiché, y compris sous l'état mappé : c'est ce qui permet
             * de voir qu'une commande produit autre chose que ce qu'on croit. */
            printRaw(jid);
            std::printf("\n");
        }

        if (found == 0) {
            std::printf("Aucune manette détectée. Branche-la, elle apparaîtra ici.\n");
        }
        std::fflush(stdout);

        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }

    /* Code inaccessible (on sort par Ctrl+C) ; présent par souci de symétrie. */
    glfwTerminate();
    return EXIT_SUCCESS;
}
