/*
 * ApplicationMenu.cpp
 * Menu de démarrage affiché dans la fenêtre (Dear ImGui) : choix de la carte à
 * charger et du démarrage immédiat de la turbine. Il remplace l'ancien launch.bat,
 * bloqué par le Contrôle intelligent des applications de Windows (un script non signé
 * qui lance un exécutable non signé). L'exe Windows étant une application fenêtrée
 * (pas de console), le menu doit vivre dans la fenêtre, cliquable au doigt. La
 * découverte des cartes vit dans ApplicationMenuMaps.cpp et l'écran de chargement
 * générique dans ApplicationLoadingScreen.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "input/Keyboard.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"
#include "ui/HudWidgets.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <string>
#include <vector>

namespace artouste::app {

bool Application::runStartupMenu() {
    /* Le curseur n'est visible QUE pendant un menu : partout ailleurs, y compris
       sur les écrans de chargement, il est masqué (voir run, ApplicationLifecycle).
       Il resurgissait au-dessus du chargement d'une carte, celui-ci s'étant allongé.
       Le rétablir ici et le remasquer en sortant fait de ce menu son seul
       propriétaire, plutôt que de compter sur l'entrée en vol pour le cacher. */
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    struct RemasquerEnSortant {
        GLFWwindow* fenetre;
        ~RemasquerEnSortant() { glfwSetInputMode(fenetre, GLFW_CURSOR, GLFW_CURSOR_HIDDEN); }
    } remasquer{m_window};

    const std::vector<MapEntry> cartes = recenserCartes(resolveAssetDir());
    if (cartes.empty()) {
        return true; /* aucune carte recensée : on laisse initScene décider (config/défaut) */
    }

    std::size_t selection = 0;     /* carte en surbrillance (défaut : la première) */
    bool turbine = false;          /* turbine et rotor déjà démarrés au lancement ? */
    bool lancer = false;           /* passe à true quand l'utilisateur valide */
    bool demoChoisi = false;       /* passe à true si l'utilisateur choisit la démo */
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

    /* Lecture des entrées, clavier et manette 1 fusionnés en six intentions :
       up/down (choisir), turb (bascule turbine), valid (démarrer), demo (lancer la
       démo), quit (quitter). Le curseur souris étant masqué en plein écran, TOUT
       doit rester accessible au clavier/à la manette -- les boutons ImGui ne sont
       qu'un raccourci pour qui a la souris, jamais le seul chemin. */
    auto lireEntrees =
        [this](bool& up, bool& down, bool& valid, bool& turb, bool& demo, bool& quit) {
            up = glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS;
            down = glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS;
            valid = glfwGetKey(m_window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                    glfwGetKey(m_window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
            turb = glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS;
            /* Lettre résolue par la disposition réelle (voir toucheImprimant) : sur
               un clavier où "D" ne serait pas à sa position QWERTY, le raccourci
               suit quand même la lettre imprimée sur la touche. */
            demo = glfwGetKey(m_window, input::toucheImprimant('d')) == GLFW_PRESS;
            quit = glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
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
                up = up || gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
                     gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -0.5f;
                down = down || gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
                       gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > 0.5f;
                valid = valid || gp.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
                turb = turb || gp.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
                demo = demo || gp.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS;
                quit = quit || gp.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
            }
        };

    auto edge = [](bool now, bool& prev) {
        const bool front = now && !prev;
        prev = now;
        return front;
    };

    /* États précédents amorcés avec l'état courant : une touche déjà tenue à l'ouverture
       du menu -- typiquement l'Échap qui vient de faire sortir du vol -- ne doit pas
       compter comme un nouvel appui, sinon le menu se refermerait aussitôt (quitter). */
    bool pvUp = false, pvDown = false, pvValid = false, pvQuit = false, pvTurb = false,
         pvDemo = false;
    glfwPollEvents();
    lireEntrees(pvUp, pvDown, pvValid, pvTurb, pvDemo, pvQuit);
    bool pvCartes = glfwGetKey(m_window, input::toucheImprimant('c')) == GLFW_PRESS;
    bool pvMaj    = glfwGetKey(m_window, input::toucheImprimant('m')) == GLFW_PRESS;

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
        if (edge(demo, pvDemo)) { /* touche D / bouton Y : lancer la démonstration */
            demoChoisi = true;
            lancer = true;
        }
        if (edge(quit, pvQuit)) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE); /* Échap dans le menu = quitter */
        }
        /* Touche C : gestionnaire de cartes. Au clavier seulement, sans raccourci
           manette : l'écran qu'elle ouvre se pilote lui aussi au clavier, et lui
           donner une entrée manette laisserait l'utilisateur devant un tableau
           qu'il ne pourrait plus parcourir. */
        const bool cartesDemandees =
            glfwGetKey(m_window, input::toucheImprimant('c')) == GLFW_PRESS;
        if (edge(cartesDemandees, pvCartes)) {
            runGestionnaireCartes();
            /* Le gestionnaire a tenu la fenêtre pendant tout ce temps, et on en
               sort par Échap : cette touche est donc encore enfoncée au retour
               ici, où elle n'a pas été vue s'abaisser. Sans réarmer les états
               précédents, le menu la prendrait pour un nouvel appui et quitterait
               le jeu. Même précaution qu'à l'ouverture du menu. */
            glfwPollEvents();
            lireEntrees(pvUp, pvDown, pvValid, pvTurb, pvDemo, pvQuit);
            pvCartes = glfwGetKey(m_window, input::toucheImprimant('c')) == GLFW_PRESS;
            pvMaj    = glfwGetKey(m_window, input::toucheImprimant('m')) == GLFW_PRESS;
        }
        /* Touche M : ouvrir la page du projet dans le navigateur, seulement quand
           une version plus récente y attend le pilote (voir MiseAJour.hpp). Au
           clavier et à la souris seulement : lire une page web n'est pas une
           manoeuvre qu'on demande à la manette. */
        const bool majDemandee = glfwGetKey(m_window, input::toucheImprimant('m')) == GLFW_PRESS;
        if (edge(majDemandee, pvMaj) && m_maj.disponible()) {
            MiseAJour::ouvrirPage();
        }

        int fbw = 0;
        int fbh = 0;
        glfwGetFramebufferSize(m_window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.09f, 0.11f, 0.13f, 1.0f); /* fond sombre neutre */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_hud.updateScale(fbw, fbh); /* police et espacements à l'échelle (avant NewFrame) */

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImVec2 centre(ImGui::GetIO().DisplaySize.x * 0.5f,
                            ImGui::GetIO().DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Artouste -- choix du vol",
                     nullptr,
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
            const std::string libelle = cartes[i].zombieOnly
                                            ? cartes[i].title
                                            : (cartes[i].dir + "  -  " + cartes[i].title);
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
            lancer = true;
        }
        ImGui::SameLine();
        /* Gestionnaire de cartes : place occupée, arbres et bâtiments par carte,
           suppression des tuiles (voir ApplicationMenuCartes.cpp). Il prend la
           fenêtre le temps qu'il faut, puis rend la main ici ; on relit ensuite
           les cartes, l'utilisateur ayant pu en effacer les tuiles. */
        if (ImGui::Button("Cartes", ImVec2(ui::hud_widgets::sc(120.0f), 0.0f))) {
            runGestionnaireCartes();
        }
        ImGui::SameLine();
        if (ImGui::Button("Quitter", ImVec2(ui::hud_widgets::sc(120.0f), 0.0f))) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
        ImGui::Separator();
        ImGui::TextDisabled("Flèches/stick : choisir   Espace : turbine   Entrée/A : démarrer   "
                            "D/Y : démo   C : cartes   Échap/B : quitter");
        ImGui::TextDisabled("%s", ARTOUSTE_VERSION_STRING);
        /* Version plus récente publiée : on la signale sans rien imposer -- le vol
           part comme d'habitude, la proposition attend en bas du menu (voir
           MiseAJour.hpp). Le bouton ouvre le navigateur ; l'adresse reste écrite
           en clair pour qui préfère la saisir lui-même. */
        if (m_maj.disponible()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.30f, 1.0f),
                               "Version %s disponible (vous avez %s).",
                               m_maj.versionPubliee().c_str(),
                               ARTOUSTE_VERSION_SEMVER);
            if (ImGui::Button("Télécharger", ImVec2(ui::hud_widgets::sc(140.0f), 0.0f))) {
                MiseAJour::ouvrirPage();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("M : ouvrir %s", PAGE_PROJET);
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }

    m_inMenu = false; /* le callback clavier de vol reprend son rôle */

    if (!lancer) {
        return false; /* fenêtre fermée ou "Quitter" : on ne charge pas la scène */
    }

    m_menuTerrain = cartes[selection].dir;
    m_menuTurbine = turbine ? 1 : 0;
    m_menuDemo = demoChoisi; /* démo demandée : lancée par initScene/applyMenuSession */
    /* Mode zombie : démarre automatiquement sur une carte dédiée (zombieOnly, ex.
       dax-arene), qui n'a alors pas d'autre usage -- un lancement normal doit y
       démarrer le combat. Lancé par initScene/applyMenuSession. */
    m_menuCombat = cartes[selection].zombieOnly;
    /* Présentation de départ, identique à chaque lancement depuis le menu : vue
       cockpit, HUD complet et livrée armée de terre. Les moteurs, eux, sont remis
       dans l'état choisi ci-dessus (turbine démarrée ou à froid) par
       initScene/applyMenuSession. La livrée n'est qu'enregistrée ici ; son
       application (setLivery) se fait au chargement de la scène. */
    m_viewMode = 1;                          /* vue cockpit, la place du pilote */
    m_prevCamView = -1;                      /* la caméra repart d'un état neuf */
    m_hudMode = ui::HudMode::Overlay;        /* HUD complet */
    m_livery = render::Livery::ArmeeDeTerre; /* livrée armée de terre (verte) */
    return true;
}

void Application::applyMenuSession() {
    m_demoFromMenu = false; /* remis à jour ci-dessous selon le choix du menu */
    /* Démo choisie au menu : elle se joue sur Arcachon. On charge donc ce terrain ici
       (startDemo le confirmera sans le recharger), puis on lance la chorégraphie plus
       bas ; la turbine et la vue de départ retenues par la démo priment. */
    if (m_menuDemo) {
        m_menuTerrain = "arcachon";
    }
    /* Terrain : rechargé s'il a changé (l'opération est coûteuse), mais AUSSI si
       ses options ont changé. Le gestionnaire de cartes a pu éteindre ses
       bâtiments ou ses tuiles pendant que la scène était en mémoire, et relancer
       la même carte doit en tenir compte : sans cela les bâtiments éteints
       restaient à l'écran. Dans tous les cas on repasse ensuite par resetToStart :
       il repose l'appareil ET remet à zéro les commandes mémorisées (collectif),
       l'assistance et l'aide au posé -- loadTerrain seul ne purge pas ces états,
       et un collectif resté haut ferait redécoller l'appareil tout seul sur la
       nouvelle carte. */
    if (!m_menuTerrain.empty()) {
        const bool carteChangee = m_menuTerrain != m_terrainName;
        const bool optionsChangees =
            !carteChangee && !m_terrainName.empty() &&
            !(optionsEffectives(m_assetsDir / "terrain" / m_terrainName) == m_optionsChargees);
        /* Un jeu de tuiles fabriqué ou supprimé ne se lit ni dans le nom de la
           carte ni dans son options.txt, alors qu'il change tout au chargement :
           le gestionnaire le signale par m_cartesRemaniees. */
        /* monuments.txt retouché à la main pendant que la scène était en mémoire :
           on compare sa date d'écriture à celle relevée au chargement. Caler une
           trentaine de monuments demande de reprendre ce fichier des dizaines de
           fois, et sans ce test il fallait relancer le simulateur à chaque essai
           pour en voir l'effet. */
        const bool monumentsChanges =
            !carteChangee && !m_terrainName.empty() &&
            dateMonuments(m_assetsDir / "terrain" / m_terrainName) != m_monumentsDate;
        if (carteChangee || optionsChangees || m_cartesRemaniees || monumentsChanges) {
            loadTerrain(m_menuTerrain);  /* remet lui-même le drapeau à faux */
        }
    }
    resetToStart();

    /* Assistance éteinte à chaque départ en vol. resetToStart ne peut pas s'en
       charger : il sert aussi aux touches R et X, où couper l'assistance
       surprendrait le pilote en plein vol. Sans cette ligne, l'interrupteur
       survivait au retour au menu et l'on repartait assisté sans l'avoir
       demandé. La démo n'est pas concernée, elle a son propre chemin (voir
       setRealFlyPhysicsEnabled dans ApplicationLoop.cpp). */
    m_assist.disable();

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

    /* On repart d'un état neutre : ni pause, ni panneau de confirmation, et une
       annonce de la tour à émettre. Ce dernier point compte surtout pour le mode
       zombie, lancé turbine chaude : le réarmement automatique attend que la
       turbine redescende sous la moitié du régime, ce qui n'arrive pas entre deux
       parties, si bien que seule la première du processus était annoncée. */
    m_paused = false;
    m_confirmReset = false;
    m_confirmDemo = false;
    resetRadioMessage();

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
            m_viewMode =
                1; /* vue bloquée en cockpit pendant le combat (voir ApplicationInput.cpp) */
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

} /* namespace artouste::app */
