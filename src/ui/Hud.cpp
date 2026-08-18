/*
 * Hud.cpp
 * Affichage tête haute avec Dear ImGui : aiguillage d'une image selon le mode
 * (render), bandeaux centraux (pause, confirmations) et sous-titres (message
 * radio, atterrissage automatique). Le cycle de vie (init/shutdown/updateScale)
 * est dans ui/hud/HudSetup.cpp, le mode quatre coins dans HudCorners.cpp, les
 * alertes de vol dans HudAlerts.cpp, le mode superposé et le repérage dans
 * HudSuperOverlay.cpp/HudLabelsMinimap.cpp.
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

namespace artouste::ui {

using namespace hud_widgets;

void Hud::render(const HudData& data,
                 HudMode mode,
                 bool paused,
                 bool confirmReset,
                 bool confirmDemo,
                 bool forceLabels) {
    if (!m_ready) {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float w = display.x;
    const float h = display.y;
    const float m = sc(14.0f); /* marge depuis les bords (à l'échelle) */

    if (mode == HudMode::Corners) {
        renderCorners(data, w, h, m);
    } else if (mode == HudMode::Overlay) {
        renderOverlay(data, w, h, m);
    }
    /* mode Off : aucun affichage de vol. */

    /* Aide à l'atterrissage : réticule et score. Elle fait partie du HUD, donc pas de
       HUD (mode Off) => pas d'aide -- sauf en démo, où on la garde comme les étiquettes
       (forceLabels), le HUD y étant éteint par mise en scène et non par choix du pilote.
       La méthode ne dessine rien de toute façon si le guidage n'est pas actif. */
    if (mode != HudMode::Off || forceLabels) {
        renderPadGuidance(data, w, h);
    }

    /* Étiquettes des lieux : affichées avec le HUD, et aussi quand on les force (démo
       HUD éteint). La minimap, elle, ne s'affiche qu'avec le HUD. */
    const bool showLabels = (mode != HudMode::Off) || forceLabels;
    if (showLabels) {
        renderLabels(data, w, h);
    }
    if (mode != HudMode::Off) {
        renderMinimap(data, mode, m);
    }

    /* Sous-titre d'un message radio : par-dessus tous les modes (y compris HUD éteint
       en démo), pour accompagner la transmission entendue. */
    renderRadioSubtitle(data, w);
    renderAutolandMessage(data, w);

    /* Alertes de vol (vortex, vitesse, taux de descente) : par-dessus tous les modes de vol (mais
       pas prioritaires sur les panneaux de confirmation/pause, dessinés ensuite). */
    renderVortexAlert(data, w, h);
    renderVitesseAlert(data, w, h);
    renderSinkRateAlert(data, w, h);
    renderHvAlert(data, w, h);
    renderCombatHud(data, mode, w, h);

    renderBanners(paused, confirmReset, confirmDemo, w, h);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Hud::renderBanners(bool paused, bool confirmReset, bool confirmDemo, float w, float h) {
    /* Les panneaux de confirmation (reset, démo) sont prioritaires sur le bandeau de pause. */
    if (confirmReset) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("confirm_reset", nullptr, HUD_FLAGS);
        ImGui::Text("       RÉINITIALISER ?");
        ImGui::Text("Replacer l'appareil au départ");
        ImGui::Text("A / O : Oui        B / N : Non");
        ImGui::End();
    } else if (confirmDemo) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("confirm_demo", nullptr, HUD_FLAGS);
        ImGui::Text("     LANCER LA DÉMO ?");
        ImGui::Text("Vol automatique (Dune du Pilat)");
        ImGui::Text("A / O : Oui        B / N : Non");
        ImGui::End();
    } else if (paused) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGui::Begin("pause", nullptr, HUD_FLAGS);
        ImGui::Text("        PAUSE");
        ImGui::Text("P : reprendre    Échap : quitter");
        ImGui::End();
    }
}

void Hud::renderRadioSubtitle(const HudData& data, float w) {
    if (data.radioMessage == nullptr || data.radioMessage[0] == '\0') {
        return;
    }
    constexpr ImGuiWindowFlags flags = HUD_FLAGS;

    /* En haut, centré, sous le ruban de cap du HUD complet : le ruban s'étend de
       y = 30 à y = 72 (bord haut 30 + hauteur 40 + fond) et il est dessiné dans le
       foreground draw list, donc PAR-DESSUS cette fenêtre ; il faut rester en dessous.
       Le bas de l'image est occupé par le rang d'instruments, à ne pas recouvrir. */
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, sc(80.0f)), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("radio_msg", nullptr, flags);
    /* Ambre "radio", distinct du vert instrument. */
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.30f, 1.0f));
    ImGui::Text(">> %s", data.radioMessage);
    ImGui::PopStyleColor();
    ImGui::End();
}

void Hud::renderAutolandMessage(const HudData& data, float w) {
    if (data.autolandMessage == nullptr || data.autolandMessage[0] == '\0') {
        return;
    }
    constexpr ImGuiWindowFlags flags = HUD_FLAGS;

    /* Même emplacement que le sous-titre radio (haut, centré, sous le ruban de cap),
       mais légèrement plus bas pour ne pas se superposer si les deux coïncident. */
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, sc(112.0f)), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("autoland_msg", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, HUD_AMBER);
    ImGui::TextUnformatted(data.autolandMessage);
    ImGui::PopStyleColor();
    ImGui::End();
}

} /* namespace artouste::ui */
