/*
 * HudSetup.cpp
 * Cycle de vie du contexte ImGui du HUD : initialisation, mise à l'échelle de
 * la police selon la taille du framebuffer, et arrêt. Le dispatch d'une image
 * (render) et les bandeaux communs sont dans ui/Hud.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "ui/Hud.hpp"
#include "ui/HudWidgets.hpp"

#include <imgui.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>

namespace artouste::ui {

void Hud::init(GLFWwindow* window) {
    if (m_ready) {
        return; /* déjà initialisé (ex. menu de démarrage) : un seul contexte ImGui */
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; /* pas de fichier de réglages sur disque */
    /* ImGui ne doit pas piloter le curseur : sans ce drapeau, son backend GLFW remet
       GLFW_CURSOR_NORMAL à chaque image (pour afficher les curseurs de survol), ce qui
       annulerait le masquage du curseur qu'on impose en plein écran (setFullscreen). */
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGui::StyleColorsDark();
    m_baseStyle = ImGui::GetStyle(); /* style de référence, remis à l'échelle dans updateScale */
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");
    m_ready = true;
}

void Hud::updateScale(int framebufferWidth, int framebufferHeight) {
    if (!m_ready || framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }
    /* Échelle calée sur la taille du framebuffer (référence 1280x720), convertie en
       unités ImGui via DisplayFramebufferScale (sur nos cibles X11/Windows le facteur
       vaut 1 ; sur un écran HiDPI où fenêtre et framebuffer diffèrent, il évite un
       double agrandissement). Le minimum des deux axes garantit que le rang de cadrans
       tient aussi en fenêtre étroite. L'arrondi au quart crée des paliers francs : la
       police n'est pas reconstruite à chaque pixel d'un redimensionnement continu.
       À 1280x720 et à tout plein écran 16:9, rien ne change par rapport à la référence. */
    const ImVec2 fbEchelle = ImGui::GetIO().DisplayFramebufferScale;
    const float wUnites = static_cast<float>(framebufferWidth) / fbEchelle.x;
    const float hUnites = static_cast<float>(framebufferHeight) / fbEchelle.y;
    const float brut = std::min(wUnites / 1280.0f, hUnites / 720.0f);
    const float scaleFactor = std::clamp(std::round(brut * 4.0f) / 4.0f, 0.75f, 3.5f);
    hud_widgets::g_scale = scaleFactor;

    /* Reconstruction de la police et remise à l'échelle des espacements uniquement au
       changement de palier, hors NewFrame (l'atlas ne doit pas bouger en pleine frame). */
    if (scaleFactor != m_builtFontScale) {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        ImFontConfig cfg;
        cfg.SizePixels = std::round(13.0f * scaleFactor); /* police rastérisée à la bonne taille */
        io.Fonts->AddFontDefault(&cfg);
        io.Fonts->Build();
        /* On détruit seulement la texture : le prochain NewFrame du backend la recrée
           depuis l'atlas reconstruit. La créer ici, avant le tout premier NewFrame,
           ferait fuir une texture (CreateDeviceObjects la recréerait par-dessus). */
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        ImGui::GetStyle() = m_baseStyle;
        ImGui::GetStyle().ScaleAllSizes(scaleFactor);
        m_builtFontScale = scaleFactor;
    }
}

void Hud::shutdown() {
    if (!m_ready) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_ready = false;
}

} /* namespace artouste::ui */
