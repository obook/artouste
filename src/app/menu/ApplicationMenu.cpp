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
#include "app/menu/ApplicationMenuEntrees.hpp"
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
    auto edge = [](bool now, bool& prev) {
        const bool front = now && !prev;
        prev = now;
        return front;
    };

    /* États précédents amorcés avec l'état courant : une touche déjà tenue à l'ouverture
       du menu -- typiquement l'Échap qui vient de faire sortir du vol -- ne doit pas
       compter comme un nouvel appui, sinon le menu se refermerait aussitôt (quitter). */
    glfwPollEvents();
    EntreesMenu precedentes = lireEntreesMenu(m_window);
    bool pvCartes = glfwGetKey(m_window, input::toucheImprimant('c')) == GLFW_PRESS;
    bool pvMaj    = glfwGetKey(m_window, input::toucheImprimant('m')) == GLFW_PRESS;

    while (glfwWindowShouldClose(m_window) == GLFW_FALSE && !lancer) {
        glfwPollEvents();

        const EntreesMenu e = lireEntreesMenu(m_window);

        if (edge(e.haut, precedentes.haut) && selection > 0) {
            --selection;
        }
        if (edge(e.bas, precedentes.bas) && selection + 1 < cartes.size()) {
            ++selection;
        }
        if (edge(e.turbine, precedentes.turbine)) {
            turbine = !turbine;
        }
        if (edge(e.valider, precedentes.valider)) {
            lancer = true;
        }
        if (edge(e.demo, precedentes.demo)) { /* touche D / bouton Y : lancer la démonstration */
            demoChoisi = true;
            lancer = true;
        }
        if (edge(e.quitter, precedentes.quitter)) {
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
            precedentes = lireEntreesMenu(m_window);
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
            if (ImGui::RadioButton(libelleCarte(cartes[i]).c_str(), selection == i)) {
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

    m_menu.terrain = cartes[selection].dir;
    m_menu.turbine = turbine ? 1 : 0;
    m_menu.demo = demoChoisi; /* démo demandée : lancée par initScene/applyMenuSession */
    /* Mode zombie : démarre automatiquement sur une carte dédiée (zombieOnly, ex.
       dax-arene), qui n'a alors pas d'autre usage -- un lancement normal doit y
       démarrer le combat. Lancé par initScene/applyMenuSession. */
    m_menu.combat = cartes[selection].zombieOnly;
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

} /* namespace artouste::app */
