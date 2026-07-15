/*
 * ApplicationLoadingScreen.cpp
 * Écran de chargement générique (une image, un message), affiché avant un
 * chargement bloquant (terrain, modèle 3D...). Appelé depuis run(),
 * initScene() et applyMenuSession(). Extrait de ApplicationMenu.cpp.
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

#include "ui/HudWidgets.hpp"

namespace artouste::app {

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
