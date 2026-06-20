/*
 * Hud.cpp
 * Construction de l'affichage tête haute avec Dear ImGui : quatre panneaux
 * dans les coins de l'écran, plus un bandeau de pause centré.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "ui/Hud.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace artouste::ui {

namespace {

/* Ouvre un petit panneau sans décor, ancré à un coin de l'écran.
 * Le pivot (0 ou 1 sur chaque axe) indique de quel coin il s'agit. */
void corner(const char* id, const ImVec2& pos, const ImVec2& pivot) {
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowBgAlpha(0.30f);
    ImGui::Begin(id, nullptr, flags);
}

}  /* namespace */

void Hud::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  /* pas de fichier de réglages sur disque */
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");
    m_ready = true;
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

void Hud::render(const HudData& data, bool showHud, bool paused) {
    if (!m_ready) {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float  w       = display.x;
    const float  h       = display.y;
    const float  m       = 14.0f;  /* marge depuis les bords */

    if (showHud) {
        corner("hud_tl", ImVec2(m, m), ImVec2(0.0f, 0.0f));
        ImGui::Text("ALT  %5.0f m", static_cast<double>(data.altitudeM));
        ImGui::Text("V/S  %+5.0f ft/min", static_cast<double>(data.varioFpm));
        ImGui::End();

        corner("hud_tr", ImVec2(w - m, m), ImVec2(1.0f, 0.0f));
        ImGui::Text("IAS  %4.0f kt", static_cast<double>(data.airspeedKt));
        ImGui::Text("HDG  %03.0f", static_cast<double>(data.headingDeg));
        ImGui::End();

        corner("hud_bl", ImVec2(m, h - m), ImVec2(0.0f, 1.0f));
        ImGui::Text("NR   %3.0f %%", static_cast<double>(data.rotorPct));
        ImGui::Text("COLL %3.0f %%", static_cast<double>(data.collectivePct));
        ImGui::End();

        corner("hud_br", ImVec2(w - m, h - m), ImVec2(1.0f, 1.0f));
        ImGui::Text("PALONNIER");
        ImGui::ProgressBar(data.pedals * 0.5f + 0.5f, ImVec2(140.0f, 0.0f), "");
        ImGui::End();
    }

    if (paused) {
        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGui::Begin("pause", nullptr, flags);
        ImGui::Text("        PAUSE");
        ImGui::Text("P : reprendre    Échap : quitter");
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  /* namespace artouste::ui */
