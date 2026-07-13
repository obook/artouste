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

#include "render/LoadedHelicopter.hpp"
#include "ui/HudWidgets.hpp"

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
    bool        demoChoisi = false; /* passe à true si l'utilisateur choisit la démo */

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

    /* Lecture des entrées, clavier et manette 1 fusionnés en six intentions :
       up/down (choisir), turb (bascule turbine), valid (démarrer), demo (lancer la
       démo), quit (quitter). */
    auto lireEntrees = [this](bool& up, bool& down, bool& valid, bool& turb, bool& demo,
                              bool& quit) {
        up    = glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS;
        down  = glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS;
        valid = glfwGetKey(m_window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                glfwGetKey(m_window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
        turb  = glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS;
        demo  = glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS;
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
            demo  = demo || gp.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS;
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
    bool pvUp = false, pvDown = false, pvValid = false, pvQuit = false, pvTurb = false,
         pvDemo = false;
    glfwPollEvents();
    lireEntrees(pvUp, pvDown, pvValid, pvTurb, pvDemo, pvQuit);

    while (glfwWindowShouldClose(m_window) == GLFW_FALSE && !lancer) {
        glfwPollEvents();

        bool up = false, down = false, valid = false, turb = false, demo = false, quit = false;
        lireEntrees(up, down, valid, turb, demo, quit);

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
        if (edge(demo, pvDemo)) {  /* touche D / bouton Y : lancer la démonstration */
            demoChoisi = true;
            lancer     = true;
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

        m_hud.updateScale(fbw, fbh);  /* police et espacements à l'échelle (avant NewFrame) */

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
        if (ImGui::Button("Démarrer", ImVec2(ui::hud_widgets::sc(160.0f), 0.0f))) {
            lancer = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Démo", ImVec2(ui::hud_widgets::sc(120.0f), 0.0f))) {
            demoChoisi = true;
            lancer     = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Quitter", ImVec2(ui::hud_widgets::sc(120.0f), 0.0f))) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
        ImGui::Separator();
        ImGui::TextDisabled(
            "Flèches/stick : choisir   Espace : turbine   Entrée/A : démarrer   D/Y : démo   Échap/B : quitter");
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
    m_menuDemo    = demoChoisi;  /* démo demandée : lancée par initScene/applyMenuSession */
    /* Présentation de départ, identique à chaque lancement depuis le menu : vue
       arrière (poursuite), HUD complet et livrée armée de terre. Les moteurs, eux,
       sont remis dans l'état choisi ci-dessus (turbine démarrée ou à froid) par
       initScene/applyMenuSession. La livrée n'est qu'enregistrée ici ; son
       application (setLivery) se fait au chargement de la scène. */
    m_viewMode    = 0;   /* vue poursuite (arrière) */
    m_prevCamView = -1;  /* la caméra repart d'un état neuf */
    m_hudMode     = ui::HudMode::Overlay;  /* HUD complet */
    m_livery      = render::Livery::ArmeeDeTerre;  /* livrée armée de terre (verte) */
    return true;
}

void Application::applyMenuSession() {
    m_demoFromMenu = false;  /* remis à jour ci-dessous selon le choix du menu */
    /* Démo choisie au menu : elle se joue sur Arcachon. On charge donc ce terrain ici
       (startDemo le confirmera sans le recharger), puis on lance la chorégraphie plus
       bas ; la turbine et la vue de départ retenues par la démo priment. */
    if (m_menuDemo) {
        m_menuTerrain = "arcachon";
    }
    /* Terrain : rechargé seulement s'il a changé (l'opération est coûteuse). Dans
       tous les cas on repasse ensuite par resetToStart : il repose l'appareil ET
       remet à zéro les commandes mémorisées (collectif), l'assistance et l'aide au
       posé -- loadTerrain seul ne purge pas ces états, et un collectif resté haut
       ferait redécoller l'appareil tout seul sur la nouvelle carte. */
    if (!m_menuTerrain.empty() && m_menuTerrain != m_terrainName) {
        loadTerrain(m_menuTerrain);
    }
    resetToStart();

    /* Livrée de départ (armée de terre) : appliquée à chaque relance depuis le menu,
       le modèle 3D persistant gardant sinon la livrée du vol précédent. */
    if (m_loadedHeli) {
        m_loadedHeli->setLivery(m_livery);
    }

    /* Turbine et rotor selon le choix du menu : démarrés d'emblée, ou à froid. */
    if (m_menuTurbine == 1) {
        m_flight.turbine().forceRunning();
    } else {
        m_flight.turbine().stopNow();
    }

    /* On repart d'un état neutre : ni pause, ni panneau de confirmation. */
    m_paused       = false;
    m_confirmReset = false;
    m_confirmDemo  = false;

    /* Démo demandée au menu : on lance la démonstration (elle repose l'appareil, force
       le démarrage rapide de la turbine et joue la chorégraphie). En sortir ramènera au
       menu (m_demoFromMenu). Sinon, on part en vol libre, démo arrêtée. */
    if (m_menuDemo) {
        m_demoFromMenu = true;
        startDemo();
    } else {
        m_demo.stop();
    }
}

void Application::renderLoadingScreen(const char* message) {
    /* Même schéma que la boucle du menu (fenêtre ImGui centrée, sans décor) : on ne
       dessine qu'une seule image, mais on force son affichage par un swap buffer
       avant de rendre la main à l'appelant, qui va enchaîner sur un chargement
       bloquant (terrain, modèle 3D...). Sans ce swap, la fenêtre resterait figée sur
       la dernière image du menu, sans aucun retour visuel pendant 3-4 s.

       Sans effet si le contexte ImGui n'est pas encore prêt (chemins sans menu :
       capture, tests scriptés via ARTOUSTE_TERRAIN/ARTOUSTE_NO_MENU) : ces chemins
       n'ont pas de fenêtre à rafraîchir pour un utilisateur. */
    if (!m_hud.ready()) {
        return;
    }
    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0.09f, 0.11f, 0.13f, 1.0f);  /* même fond neutre que le menu */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_hud.updateScale(fbw, fbh);  /* police et espacements à l'échelle (avant NewFrame) */

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    /* Taille calculée explicitement (plutôt que ImGuiWindowFlags_AlwaysAutoResize) :
       cette fenêtre est réutilisée d'un appel à l'autre avec des messages de longueur
       différente ("Chargement...", "Chargement du terrain...", ...), et comme chaque
       appel ne dessine qu'une seule image, l'auto-resize -- qui rattrape la bonne
       taille avec un cadre de retard -- tronquerait le texte le temps d'un appel. */
    const ImVec2 pad(ui::hud_widgets::sc(24.0f), ui::hud_widgets::sc(16.0f));
    const ImVec2 textSize = ImGui::CalcTextSize(message);
    const ImVec2 winSize(textSize.x + pad.x * 2.0f, textSize.y + pad.y * 2.0f);
    const ImVec2 centre(ImGui::GetIO().DisplaySize.x * 0.5f,
                        ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);
    ImGui::Begin("##chargement", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    ImGui::TextUnformatted(message);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
    /* Vide la file d'événements système : sans ça, l'OS peut estimer la fenêtre
       "ne répond pas" pendant le chargement bloquant qui suit. */
    glfwPollEvents();
}

}  /* namespace artouste::app */
