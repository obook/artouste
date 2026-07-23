/*
 * ApplicationMenu.cpp
 * Menu de démarrage affiché dans la fenêtre (Dear ImGui) : choix de la carte à
 * charger et du démarrage immédiat de la turbine. Il remplace l'ancien launch.bat,
 * bloqué par le Contrôle intelligent des applications de Windows (un script non signé
 * qui lance un exécutable non signé). L'exe Windows étant une application fenêtrée
 * (pas de console), le menu doit vivre dans la fenêtre, cliquable au doigt.
 * L'écran de chargement générique vit dans ApplicationLoadingScreen.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"
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
    bool        zombieCapable = false;  /* présence de zombies.txt : mode zombie proposé */
    /* Présence de zombie_only.txt : carte dédiée au mode zombie (ex. dax-arene),
       sans autre usage -- lancer normalement (Démarrer/Entrée/A) suffit à
       démarrer le combat, pas besoin du bouton "Mode Zombie" ni de Z/LB. */
    bool        zombieOnly = false;
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
   trié par nom pour un ordre stable (la première est le choix par défaut), sauf les
   cartes dédiées au mode zombie (zombieOnly, ex. Happy DeathHour/dax-arene) qui
   passent systématiquement en dernier -- ce sont des arènes à part, pas des cartes de
   tourisme normales, et les mélanger dans le tri alphabétique n'aurait pas de sens. */
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
            /* Présence de zombies.txt : même mécanisme que les autres fichiers
               optionnels par carte (exclusions.txt, hapi.txt) -- voir
               render::Terrain et app::WaveManager, qui le lisent chacun de
               leur côté sans dépendre l'un de l'autre. */
            const bool zombieCapable = fs::exists(e.path() / "zombies.txt");
            /* Même mécanisme, pour distinguer une carte dédiée (dax-arene) d'une
               carte normale qui proposerait le mode zombie en option. */
            const bool zombieOnly = fs::exists(e.path() / "zombie_only.txt");
            cartes.push_back({nom, lireTitreCarte(txt, nom), zombieCapable, zombieOnly});
        }
    }
    std::sort(cartes.begin(), cartes.end(), [](const MapEntry& a, const MapEntry& b) {
        if (a.zombieOnly != b.zombieOnly) {
            return !a.zombieOnly;  /* les cartes dédiées au mode zombie passent en dernier */
        }
        return a.dir < b.dir;
    });
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
    bool        modeZombieChoisi = false;  /* passe à true si l'utilisateur choisit le mode zombie */

    /* Le menu lit les entrées directement dans GLFW (clavier + manette) plutôt que la
       navigation interne d'ImGui : comportement déterministe, indépendant du focus et du
       compositeur. La souris reste gérée par les widgets ImGui. m_inMenu neutralise le
       callback clavier de vol le temps du menu (sinon T, L, C... agiraient sur une scène
       pas encore initialisée). */
    m_inMenu = true;

    /* Silence pendant le menu : on suspend les sons de vol (moteur, rotor, son de
       démarrage). Sans danger au lancement (l'audio n'est pas encore initialisé :
       setPaused sort aussitôt). Au retour en vol, updateAudio rappelle setPaused(false)
       et tout reprend. */
    m_audio.setPaused(true);
    /* La radio, elle, est coupée pour de bon (pas seulement suspendue) : setPaused
       laisse le flux réseau actif en arrière-plan, ce qui le ferait reprendre tout seul
       au retour en vol -- même si l'utilisateur n'a rien redemandé. Quitter une carte
       pour le menu doit couper la radio, comme si on l'éteignait. */
    m_audio.stopRadio();

    /* Lecture des entrées, clavier et manette 1 fusionnés en sept intentions :
       up/down (choisir), turb (bascule turbine), valid (démarrer), demo (lancer la
       démo), zombie (lancer le mode zombie, si la carte le propose), quit (quitter).
       Le curseur souris étant masqué en plein écran, TOUT doit rester accessible au
       clavier/à la manette -- les boutons ImGui ne sont qu'un raccourci pour qui a
       la souris, jamais le seul chemin. */
    auto lireEntrees = [this](bool& up, bool& down, bool& valid, bool& turb, bool& demo,
                              bool& zombie, bool& quit) {
        up     = glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS;
        down   = glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS;
        valid  = glfwGetKey(m_window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                glfwGetKey(m_window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
        turb   = glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS;
        demo   = glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS;
        zombie = glfwGetKey(m_window, GLFW_KEY_Z) == GLFW_PRESS;
        quit   = glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
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
            up     = up || gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
                    gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -0.5f;
            down   = down || gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
                    gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > 0.5f;
            valid  = valid || gp.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
            turb   = turb || gp.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
            demo   = demo || gp.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS;
            zombie = zombie || gp.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS;
            quit   = quit || gp.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
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
         pvDemo = false, pvZombie = false;
    glfwPollEvents();
    lireEntrees(pvUp, pvDown, pvValid, pvTurb, pvDemo, pvZombie, pvQuit);

    while (glfwWindowShouldClose(m_window) == GLFW_FALSE && !lancer) {
        glfwPollEvents();

        bool up = false, down = false, valid = false, turb = false, demo = false,
             zombie = false, quit = false;
        lireEntrees(up, down, valid, turb, demo, zombie, quit);

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
        /* touche Z / bouton LB : lancer le mode zombie -- seulement si la carte
           en surbrillance le propose EN OPTION (même condition que le bouton
           ImGui). Une carte dédiée (zombieOnly, ex. dax-arene) n'a pas besoin
           de ce raccourci : un lancement normal suffit (voir plus bas). */
        if (edge(zombie, pvZombie) && cartes[selection].zombieCapable &&
            !cartes[selection].zombieOnly) {
            modeZombieChoisi = true;
            lancer           = true;
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
            /* Le nom de dossier technique (ex. "dax-arene") n'a pas sa place devant une
               arène dédiée au mode zombie (ex. "Happy DeathHour") : titre seul, sans
               préfixe, pour ces cartes-là. Les autres gardent le préfixe (utile pour
               retrouver le dossier à faire correspondre à la clé "terrain" de
               config.txt). */
            const std::string libelle =
                cartes[i].zombieOnly ? cartes[i].title : (cartes[i].dir + "  -  " + cartes[i].title);
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
        /* Mode zombie EN OPTION : seulement proposé sur une carte qui fournit des
           points de spawn (zombies.txt) SANS être dédiée au mode zombie (voir
           zombieOnly juste en dessous) -- une carte dédiée (dax-arene) lance le
           combat par un démarrage normal, ce bouton serait redondant. */
        if (cartes[selection].zombieCapable && !cartes[selection].zombieOnly) {
            ImGui::SameLine();
            if (ImGui::Button("Mode Zombie", ImVec2(ui::hud_widgets::sc(140.0f), 0.0f))) {
                modeZombieChoisi = true;
                lancer           = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Quitter", ImVec2(ui::hud_widgets::sc(120.0f), 0.0f))) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
        ImGui::Separator();
        ImGui::TextDisabled(
            "Flèches/stick : choisir   Espace : turbine   Entrée/A : démarrer   D/Y : démo   Échap/B : quitter");
        if (cartes[selection].zombieCapable && !cartes[selection].zombieOnly) {
            ImGui::TextDisabled("Z / LB : mode zombie");
        }
        ImGui::TextDisabled("%s", ARTOUSTE_VERSION_STRING);
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
    /* Mode zombie demandé : soit explicitement (bouton/Z/LB), soit implicitement
       si la carte choisie est dédiée au mode zombie (zombieOnly, ex. dax-arene) --
       elle n'a alors pas d'autre usage, un lancement normal doit y démarrer le
       combat. Lancé par initScene/applyMenuSession. */
    m_menuCombat = modeZombieChoisi || cartes[selection].zombieOnly;
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

    /* Mode zombie demandé au menu : démarre (ou arrête) la session de combat sur
       le terrain courant, comme au premier lancement (voir ApplicationScene.cpp
       ::initScene). Sans effet si la carte n'a pas de zombies.txt. */
    if (m_menuCombat) {
        m_combat.start(m_assetsDir / "terrain" / m_terrainName,
                       [this](float x, float z) { return m_terrain->heightAt(x, z); });
        if (m_combat.active()) {
            m_viewMode = 1;  /* vue bloquée en cockpit pendant le combat (voir ApplicationInput.cpp) */
            /* Turbine et rotor déjà au régime, quel que soit le choix du menu (voir
               ApplicationScene::initScene, même logique au premier lancement). */
            m_flight.turbine().forceRunning();
        }
    } else {
        m_combat.stop();
    }

    /* Après m_combat.start()/stop() ci-dessus : réévalue le cycle jour/nuit depuis
       la config (ou la nuit figée d'une arène dédiée) -- indispensable ici aussi,
       sans quoi revenir d'une telle arène vers une carte normale garderait le
       temps figé pour le reste de la session (voir ApplicationScene::initScene). */
    applySunSchedule();
}

}  /* namespace artouste::app */
