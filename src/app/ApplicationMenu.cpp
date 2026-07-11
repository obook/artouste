/*
 * ApplicationMenu.cpp
 * Menu de démarrage affiché dans la fenêtre (Dear ImGui) : choix de la carte à
 * charger et du démarrage immédiat de la turbine. Il remplace l'ancien launch.bat,
 * bloqué par le Contrôle intelligent des applications de Windows (un script non signé
 * qui lance un exécutable non signé). L'exe Windows étant une application fenêtrée
 * (pas de console), le menu doit vivre dans la fenêtre, cliquable au doigt.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace artouste::app {

namespace {

/* Une carte proposée au menu : nom du sous-dossier (valeur pour le terrain) et libellé
   lisible affiché à l'utilisateur. */
struct MapEntry {
    std::string dir;    /* nom du sous-dossier de assets/terrain (= nom du terrain) */
    std::string title;  /* libellé lisible, tiré de la première ligne de terrain.txt */
};

/* Titre lisible d'une carte : première ligne de son terrain.txt, débarrassée du préfixe
   de commentaire "# Terrain Artouste - ". À défaut, on renvoie le nom du dossier. */
std::string lireTitreCarte(const std::filesystem::path& terrainTxt, const std::string& repli) {
    std::ifstream f(terrainTxt);
    std::string   ligne;
    if (f && std::getline(f, ligne)) {
        const std::string prefixe = "# Terrain Artouste - ";
        if (const auto pos = ligne.find(prefixe); pos != std::string::npos) {
            return ligne.substr(pos + prefixe.size());
        }
        if (ligne.rfind("# ", 0) == 0) {  /* commentaire sans le préfixe attendu */
            return ligne.substr(2);
        }
        if (!ligne.empty()) {
            return ligne;
        }
    }
    return repli;
}

/* Recense les cartes : chaque sous-dossier de assets/terrain contenant un terrain.txt,
   trié par nom pour un ordre stable (la première est le choix par défaut). */
std::vector<MapEntry> recenserCartes(const std::filesystem::path& assets) {
    namespace fs = std::filesystem;
    std::vector<MapEntry> cartes;
    std::error_code       ec;
    for (const auto& e : fs::directory_iterator(assets / "terrain", ec)) {
        if (!e.is_directory()) {
            continue;
        }
        const fs::path txt = e.path() / "terrain.txt";
        if (fs::exists(txt)) {
            const std::string nom = e.path().filename().string();
            cartes.push_back({nom, lireTitreCarte(txt, nom)});
        }
    }
    std::sort(cartes.begin(), cartes.end(),
              [](const MapEntry& a, const MapEntry& b) { return a.dir < b.dir; });
    return cartes;
}

}  /* namespace */

bool Application::runStartupMenu() {
    const std::vector<MapEntry> cartes = recenserCartes(resolveAssetDir());
    if (cartes.empty()) {
        return true;  /* aucune carte recensée : on laisse initScene décider (config/défaut) */
    }

    std::size_t selection = 0;      /* carte en surbrillance (défaut : la première) */
    bool        turbine   = false;  /* turbine et rotor déjà démarrés au lancement ? */
    bool        lancer    = false;  /* passe à true quand l'utilisateur valide */

    /* Le menu lit les entrées directement dans GLFW (clavier + manette) plutôt que la
       navigation interne d'ImGui : comportement déterministe, indépendant du focus et du
       compositeur. La souris reste gérée par les widgets ImGui. m_inMenu neutralise le
       callback clavier de vol le temps du menu (sinon T, L, C... agiraient sur une scène
       pas encore initialisée). */
    m_inMenu = true;

    /* Silence pendant le menu : on suspend les sons de vol (moteur, rotor, son de
       démarrage) et le flux radio. Sans danger au lancement (l'audio n'est pas encore
       initialisé : setPaused sort aussitôt). Au retour en vol, updateAudio rappelle
       setPaused(false) et tout reprend. */
    m_audio.setPaused(true);

    /* Lecture des entrées, clavier et manette 1 fusionnés en cinq intentions :
       up/down (choisir), turb (bascule turbine), valid (démarrer), quit (quitter). */
    auto lireEntrees = [this](bool& up, bool& down, bool& valid, bool& turb, bool& quit) {
        up    = glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS;
        down  = glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS;
        valid = glfwGetKey(m_window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                glfwGetKey(m_window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
        turb  = glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS;
        quit  = glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        /* Première manette reconnue, quel que soit son slot (un périphérique virtuel
           peut occuper le slot 0 et reléguer la vraie manette plus loin) -- même logique
           qu'en vol (input::Gamepad). */
        int jid = -1;
        for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; ++j) {
            if (glfwJoystickIsGamepad(j) == GLFW_TRUE) {
                jid = j;
                break;
            }
        }
        GLFWgamepadstate gp;
        if (jid >= 0 && glfwGetGamepadState(jid, &gp) == GLFW_TRUE) {
            up    = up || gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
                    gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -0.5f;
            down  = down || gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
                    gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > 0.5f;
            valid = valid || gp.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
            turb  = turb || gp.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
            quit  = quit || gp.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
        }
    };

    auto edge = [](bool now, bool& prev) {
        const bool front = now && !prev;
        prev             = now;
        return front;
    };

    /* États précédents amorcés avec l'état courant : une touche déjà tenue à l'ouverture
       du menu -- typiquement l'Échap qui vient de faire sortir du vol -- ne doit pas
       compter comme un nouvel appui, sinon le menu se refermerait aussitôt (quitter). */
    bool pvUp = false, pvDown = false, pvValid = false, pvQuit = false, pvTurb = false;
    glfwPollEvents();
    lireEntrees(pvUp, pvDown, pvValid, pvTurb, pvQuit);

    while (glfwWindowShouldClose(m_window) == GLFW_FALSE && !lancer) {
        glfwPollEvents();

        bool up = false, down = false, valid = false, turb = false, quit = false;
        lireEntrees(up, down, valid, turb, quit);

        if (edge(up, pvUp) && selection > 0) {
            --selection;
        }
        if (edge(down, pvDown) && selection + 1 < cartes.size()) {
            ++selection;
        }
        if (edge(turb, pvTurb)) {
            turbine = !turbine;
        }
        if (edge(valid, pvValid)) {
            lancer = true;
        }
        if (edge(quit, pvQuit)) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);  /* Échap dans le menu = quitter */
        }

        int fbw = 0;
        int fbh = 0;
        glfwGetFramebufferSize(m_window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.09f, 0.11f, 0.13f, 1.0f);  /* fond sombre neutre */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImVec2 centre(ImGui::GetIO().DisplaySize.x * 0.5f,
                            ImGui::GetIO().DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Artouste -- choix du vol", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings);

        ImGui::TextUnformatted("Simulateur Alouette II SE.3130");
        ImGui::Separator();
        ImGui::TextUnformatted("Carte :");
        for (std::size_t i = 0; i < cartes.size(); ++i) {
            const std::string libelle = cartes[i].dir + "  -  " + cartes[i].title;
            if (ImGui::RadioButton(libelle.c_str(), selection == i)) {
                selection = i;
            }
        }
        ImGui::Separator();
        ImGui::Checkbox("Turbine et rotor déjà démarrés (décollage immédiat)", &turbine);
        ImGui::Separator();
        if (ImGui::Button("Démarrer", ImVec2(160.0f, 0.0f))) {
            lancer = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Quitter", ImVec2(120.0f, 0.0f))) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
        ImGui::Separator();
        ImGui::TextDisabled(
            "Flèches/stick : choisir   Espace : turbine   Entrée/A : démarrer   Échap/B : quitter");
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }

    m_inMenu = false;  /* le callback clavier de vol reprend son rôle */

    if (!lancer) {
        return false;  /* fenêtre fermée ou "Quitter" : on ne charge pas la scène */
    }

    m_menuTerrain = cartes[selection].dir;
    m_menuTurbine = turbine ? 1 : 0;
    return true;
}

void Application::applyMenuSession() {
    /* Terrain : rechargé seulement s'il a changé (l'opération est coûteuse et
       repositionne déjà l'appareil au parking). Même carte : on repose l'appareil. */
    if (!m_menuTerrain.empty() && m_menuTerrain != m_terrainName) {
        loadTerrain(m_menuTerrain);
    } else {
        resetToStart();
    }

    /* Turbine et rotor selon le choix du menu : démarrés d'emblée, ou à froid. */
    if (m_menuTurbine == 1) {
        m_flight.turbine().forceRunning();
    } else {
        m_flight.turbine().stopNow();
    }

    /* On repart d'un état neutre : ni démo, ni pause, ni panneau de confirmation. */
    m_demo.stop();
    m_paused       = false;
    m_confirmReset = false;
    m_confirmDemo  = false;
}

}  /* namespace artouste::app */
