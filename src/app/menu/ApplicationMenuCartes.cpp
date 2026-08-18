/*
 * ApplicationMenuCartes.cpp
 * Gestionnaire de cartes : l'écran qui montre ce que chaque carte occupe sur
 * le disque et laisse en disposer, ouvert depuis le menu de démarrage.
 *
 * Une carte est un empilement (voir docs/DISTRIBUTION.md) : un socle léger,
 * des bâtiments facultatifs, et des tuiles d'orthophoto qui pèsent mille fois
 * plus. Sans un endroit pour voir et défaire tout cela, un simulateur de 45 Mo
 * finit par occuper plusieurs gigaoctets.
 *
 * Ce fichier ne tient que la boucle : l'état, les touches et les quatre blocs
 * d'affichage vivent dans menu/cartes/.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "app/menu/cartes/EcranCartes.hpp"
#include "ui/HudWidgets.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace artouste::app {

void Application::runGestionnaireCartes() {
    /* Curseur visible le temps de cet écran, masqué en sortant : mêmes raisons
       que dans runStartupMenu, qui commente le partage. */
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    struct RemasquerEnSortant {
        GLFWwindow* fenetre;
        ~RemasquerEnSortant() { glfwSetInputMode(fenetre, GLFW_CURSOR, GLFW_CURSOR_HIDDEN); }
    } remasquer{m_window};

    /* L'écran s'ouvre AVANT le chargement de la scène : m_assetsDir n'est pas
       encore renseigné, il faut localiser les ressources soi-même. */
    ecran_cartes::Etat etat;
    etat.assets = m_assetsDir.empty() ? resolveAssetDir() : m_assetsDir;
    etat.cartes = inventorierCartes(etat.assets);
    if (etat.cartes.empty()) {
        std::fprintf(stderr, "[cartes] aucune carte trouvée dans %s\n",
                     (etat.assets / "terrain").string().c_str());
        return;
    }
    etat.racineTuiles = racineTuiles();
    /* Ce que vaut un réglage qu'aucune carte n'a pris pour elle : même calcul
       que dans inventorierCartes, dont il doit rester le reflet exact. */
    etat.arbresGeneral = m_config.trees && std::getenv("ARTOUSTE_NO_TREES") == nullptr;

    int images = 0;

    while (!etat.fini && glfwWindowShouldClose(m_window) == 0) {
        glfwPollEvents();
        ecran_cartes::traiterClavier(m_window, etat);

        /* Les actions posent leurs demandes, la boucle les exécute : elle seule
           sait refaire l'inventaire et prévenir le reste du programme. */
        if (etat.disqueRemanie) {
            m_cartesRemaniees  = true;
            etat.disqueRemanie = false;
        }
        if (etat.refaireInventaire && etat.inventaireCarteSeule &&
            etat.selection < etat.cartes.size()) {
            /* Seule la carte remaniée est remesurée. Sans barre de progression :
               il n'y a plus de suite de cartes à situer, seulement une attente
               courte. */
            /* Nom et titre copiés avant l'appel : la ligne remplacée est celle
               d'où ils viennent. */
            const MapEntry choisie{etat.cartes[etat.selection].dir,
                                   etat.cartes[etat.selection].titre, false};
            etat.cartes[etat.selection] = inventorierCarte(etat.assets, choisie, -1.0f);
            etat.refaireInventaire    = false;
            etat.inventaireCarteSeule = false;
        }
        if (etat.refaireInventaire) {
            etat.cartes            = inventorierCartes(etat.assets);
            etat.refaireInventaire = false;
            if (etat.cartes.empty()) {
                /* Les cartes ont disparu du disque pendant l'écran : sans cette
                   sortie, size() - 1 vaudrait SIZE_MAX et courante() lirait hors
                   du vecteur. */
                std::fprintf(stderr, "[cartes] plus aucune carte : retour au menu.\n");
                etat.fini = true;
                break;
            }
            etat.selection = std::min(etat.selection, etat.cartes.size() - 1);
        }

        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(m_window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.09f, 0.11f, 0.13f, 1.0f); /* même fond que le menu */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_hud.updateScale(fbw, fbh);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImVec2 centre(ImGui::GetIO().DisplaySize.x * 0.5f,
                            ImGui::GetIO().DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        /* Largeur figée : sinon la fenêtre s'ajuste au texte le plus long et
           bondit d'une carte à l'autre. */
        const float largeur = ui::hud_widgets::sc(780.0f);
        ImGui::SetNextWindowSizeConstraints(ImVec2(largeur, 0.0f),
                                            ImVec2(largeur, ImGui::GetIO().DisplaySize.y));
        ImGui::Begin("Artouste -- cartes",
                     nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings);

        ecran_cartes::dessinerEntete(etat);
        ImGui::Separator();
        ecran_cartes::dessinerTableau(etat);
        ImGui::Separator();

        /* Avancement lu UNE fois par image : le détail et les actions doivent
           parler du même instant. */
        const cartes::Avancement av = etat.fabrique.avancement();

        /* Hauteur figée, comme la largeur : le nombre de lignes change d'une
           carte à l'autre, et la fenêtre centrée se déplaçait à chaque flèche.
           On réserve la place du cas le plus haut ; le reste défile. */
        const float reserve = 9.0f * ImGui::GetTextLineHeightWithSpacing() +
                              3.0f * ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("zone", ImVec2(0.0f, reserve));
        ecran_cartes::dessinerDetail(etat, av);
        ecran_cartes::dessinerActions(etat, av);
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextDisabled("Flèches : choisir   Entrée : fabriquer les tuiles   "
                            "Suppr : les supprimer");
        ImGui::TextDisabled("L : fabriquer relief 3D   Maj+Suppr : le supprimer");
        ImGui::TextDisabled("A : arbres   B : bâtiments   T : allumer ou éteindre les tuiles   "
                            "R : réglages par défaut   Échap : retour");
        ImGui::TextDisabled("Les arbres n'occupent aucun disque : ils coûtent des images par "
                            "seconde, pas des mégaoctets.");
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        /* Capture de diagnostic, comme ARTOUSTE_SCREENSHOT pour le vol. Lit le
           tampon arrière avant l'échange, quand il porte encore l'image. */
        if (const char* shot = std::getenv("ARTOUSTE_SHOT_CARTES");
            shot != nullptr && shot[0] != '\0' && ++images >= 3) {
            glFinish();
            std::vector<unsigned char> px(static_cast<std::size_t>(fbw) *
                                          static_cast<std::size_t>(fbh) * 3u);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, fbw, fbh, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            stbi_flip_vertically_on_write(1);
            stbi_write_png(shot, fbw, fbh, 3, px.data(), fbw * 3);
            std::printf("[cartes] capture %s\n", shot);
            etat.fini = true;
        }

        glfwSwapBuffers(m_window);
    }
}

} /* namespace artouste::app */
