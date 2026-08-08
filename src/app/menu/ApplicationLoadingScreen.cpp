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

#include <algorithm>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "render/Texture.hpp"
#include "ui/HudWidgets.hpp"

namespace artouste::app {

namespace {

/* Dessine l'image en "cover" : elle couvre tout le cadre en gardant ses
   proportions, quitte à déborder sur un côté. Le contraire ("contain")
   laisserait des bandes noires, moins soignées pour un écran d'attente. */
void dessinerFondCouvrant(unsigned int texture, float fbw, float fbh, float aspectImage) {
    const float aspectCadre = fbw / fbh;
    float       l = fbw, h = fbh;
    if (aspectImage > aspectCadre) {
        l = fbh * aspectImage;  /* image plus large : on déborde à gauche et à droite */
    } else {
        h = fbw / aspectImage;  /* image plus haute : on déborde en haut et en bas */
    }
    const ImVec2 debut{0.5f * (fbw - l), 0.5f * (fbh - h)};
    ImGui::GetBackgroundDrawList()->AddImage(static_cast<ImTextureID>(texture), debut,
                                            ImVec2{debut.x + l, debut.y + h},
                                            /* l'image est retournée par stb_image au
                                               chargement (origine OpenGL en bas) : on
                                               inverse donc V pour la remettre à
                                               l'endroit. */
                                            ImVec2{0.0f, 1.0f}, ImVec2{1.0f, 0.0f});
}

}  /* namespace */

void Application::renderLoadingScreen(const char* message, float progression) {
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

    /* Affiche du jeu en fond. Chargée au premier écran d'attente et gardée
       ensuite : elle resservira à chaque changement de carte, et la charger au
       démarrage retarderait justement le moment où l'on peut afficher quelque
       chose. Absente, on garde le fond neutre.

       Le tout premier appel a lieu juste après le menu, avant initScene() :
       m_assetsDir n'est pas encore renseigné (même piège qu'ApplicationScene.cpp
       et ApplicationMenuCartes.cpp, même contournement). Sans lui, cette toute
       première tentative échouait sur un chemin relatif ("textures/..." au lieu
       de "assets/textures/..."), et comme l'image ne se charge qu'une fois, l'échec
       restait pour le reste de la session même une fois m_assetsDir renseigné. */
    if (!m_loadingImage) {
        const std::filesystem::path assets = m_assetsDir.empty() ? resolveAssetDir() : m_assetsDir;
        m_loadingImage = std::make_unique<render::Texture>(assets / "textures" / "chargement.jpg");
    }
    if (m_loadingImage->valid() && m_loadingImage->height() > 0) {
        dessinerFondCouvrant(m_loadingImage->id(), static_cast<float>(fbw),
                             static_cast<float>(fbh),
                             static_cast<float>(m_loadingImage->width()) /
                                 static_cast<float>(m_loadingImage->height()));
    }

    /* Taille calculée explicitement (plutôt que ImGuiWindowFlags_AlwaysAutoResize) :
       cette fenêtre est réutilisée d'un appel à l'autre avec des messages de longueur
       différente ("Chargement...", "Chargement du terrain...", ...), et comme chaque
       appel ne dessine qu'une seule image, l'auto-resize -- qui rattrape la bonne
       taille avec un cadre de retard -- tronquerait le texte le temps d'un appel. */
    const ImVec2 pad(ui::hud_widgets::sc(24.0f), ui::hud_widgets::sc(16.0f));
    const ImVec2 textSize = ImGui::CalcTextSize(message);
    /* La barre est plus large que le message et s'ajoute sous lui : la fenêtre
       grandit dans les deux sens quand une progression est fournie. */
    const bool   avecBarre  = (progression >= 0.0f);
    const float  largeBarre = ui::hud_widgets::sc(320.0f);
    const float  hautBarre  = ui::hud_widgets::sc(14.0f);
    /* L'écart entre le message et la barre est celui d'ImGui, pas notre marge :
       le prendre à part laissait un vide sous la barre. */
    const float  ecart = ImGui::GetStyle().ItemSpacing.y;
    const ImVec2 winSize(std::max(textSize.x, avecBarre ? largeBarre : 0.0f) + pad.x * 2.0f,
                         textSize.y + (avecBarre ? ecart + hautBarre : 0.0f) + pad.y * 2.0f);
    const ImVec2 centre(ImGui::GetIO().DisplaySize.x * 0.5f,
                        ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);
    /* Panneau translucide : l'affiche reste lisible derrière, et le message se
       détache quand même. Opaque, il faisait tache au milieu de l'image. */
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.06f, 0.08f, 0.72f));
    ImGui::Begin("##chargement", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    ImGui::TextUnformatted(message);
    if (avecBarre) {
        /* Barre ambre sur fond sombre, les couleurs d'instrument du projet.
           Sans texte surimprimé : le pourcentage n'apprend rien de plus que la
           longueur de la barre et ajoute du bruit. */
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.96f, 0.63f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.08f, 0.10f, 0.85f));
        ImGui::ProgressBar(std::min(1.0f, progression), ImVec2(largeBarre, hautBarre), "");
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
    /* Vide la file d'événements système : sans ça, l'OS peut estimer la fenêtre
       "ne répond pas" pendant le chargement bloquant qui suit. */
    glfwPollEvents();
}

}  /* namespace artouste::app */
