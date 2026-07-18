/*
 * HudCombat.cpp
 * Mode zombie : panneau vie/munitions/vague/chrono/plafond d'altitude, et
 * bandeau de fin de partie (score, retour au menu). Extrait de Hud.cpp comme
 * les autres sous-affichages (HudPadGuidance.cpp, HudSuperOverlay.cpp...).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "ui/Hud.hpp"

#include "ui/HudWidgets.hpp"

#include <imgui.h>

#include <cstdio>

namespace artouste::ui {

using namespace hud_widgets;

namespace {
constexpr ImGuiWindowFlags COMBAT_FLAGS =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;
}  /* namespace */

void Hud::renderCombatHud(const HudData& data, HudMode mode, float w, float h) {
    if (!data.combat.active) {
        return;
    }

    /* Mire du canon fixe : croix + cercle à l'endroit où pointent réellement
       les roquettes (voir Application::fillHud). Le canon ne suit pas le regard
       (pas de viseur mobile) : sans repère, impossible de savoir où viser.
       Absente si le point tombe hors du cadre (canon qui ne vise pas dans la
       direction actuellement regardée par la caméra, par exemple en vue
       orbite). Dessinée sur le calque de premier plan, comme les étiquettes
       de lieux (renderLabels), pas dans une fenêtre ImGui. */
    if (data.combat.reticleOnScreen) {
        ImDrawList* dl  = ImGui::GetForegroundDrawList();
        const ImVec2 c(data.combat.reticleFx * w, data.combat.reticleFy * h);
        const ImU32  col = IM_COL32(255, 60, 60, 220);
        dl->AddCircle(c, sc(14.0f), col, 24, sc(1.5f));
        dl->AddLine(ImVec2(c.x - sc(22.0f), c.y), ImVec2(c.x - sc(8.0f), c.y), col, sc(1.5f));
        dl->AddLine(ImVec2(c.x + sc(8.0f), c.y), ImVec2(c.x + sc(22.0f), c.y), col, sc(1.5f));
        dl->AddLine(ImVec2(c.x, c.y - sc(22.0f)), ImVec2(c.x, c.y - sc(8.0f)), col, sc(1.5f));
        dl->AddLine(ImVec2(c.x, c.y + sc(8.0f)), ImVec2(c.x, c.y + sc(22.0f)), col, sc(1.5f));
        dl->AddCircleFilled(c, sc(1.5f), col);
    }

    /* Placement selon le mode de HUD, pour ne recouvrir aucun instrument :
       - HUD 4 coins (Corners) : bas de l'écran, centré, entre les quatre coins.
       - HUD complet (Overlay) : le rang de cadrans ronds occupe justement le bas
         au centre (voir HudSuperOverlay.cpp, y = h - 70). On cale alors la carte
         sur le bord droit, à mi-hauteur, zone libre de ce mode (rubans en haut
         et à gauche, cadrans en bas, voyants en bas à gauche). */
    if (mode == HudMode::Overlay) {
        ImGui::SetNextWindowPos(ImVec2(w - sc(14.0f), h * 0.5f), ImGuiCond_Always,
                                ImVec2(1.0f, 0.5f));
    } else {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h - sc(90.0f)), ImGuiCond_Always,
                                ImVec2(0.5f, 1.0f));
    }
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("combat_hud", nullptr, COMBAT_FLAGS);

    ImGui::PushStyleColor(ImGuiCol_Text, HUD_GREEN);
    ImGui::Text("MANCHE %d   SCORE %d", data.combat.wave, data.combat.score);
    ImGui::Text("ZTUES %d", data.combat.kills);
    const int totalSec = static_cast<int>(data.combat.elapsedS);
    ImGui::Text("TEMPS %02d:%02d", totalSec / 60, totalSec % 60);
    ImGui::PopStyleColor();

    /* Vie : verte au-dessus de 60 %, ambre entre 30 et 60 %, rouge en dessous
       -- mêmes seuils visuels que les alarmes des cadrans (voir HudAlarms.hpp). */
    const ImU32 healthColor = (data.combat.healthPct < 0.3f)   ? HUD_RED
                            : (data.combat.healthPct < 0.6f) ? HUD_AMBER
                                                              : HUD_GREEN;
    ImGui::PushStyleColor(ImGuiCol_Text, healthColor);
    ImGui::Text("VIE   %3.0f %%", static_cast<double>(data.combat.healthPct * 100.0f));
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, HUD_GREEN);
    ImGui::Text("MUN   %3d / %3d", data.combat.ammoCurrent, data.combat.ammoMax);
    ImGui::PopStyleColor();

    /* Plafond d'altitude : sans ce repère, le joueur ne peut pas deviner
       pourquoi il se fait toucher (sous le plafond) ou non (au-dessus). */
    ImGui::PushStyleColor(ImGuiCol_Text, data.combat.belowCeiling ? HUD_AMBER : HUD_GREEN);
    ImGui::TextUnformatted(data.combat.belowCeiling ? "SOUS LE PLAFOND - VULNERABLE"
                                                    : "HORS DE PORTEE DES ZOMBIES");
    ImGui::PopStyleColor();

    ImGui::End();

    if (data.combat.gameOver) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.80f);
        ImGui::Begin("combat_gameover", nullptr, COMBAT_FLAGS);
        ImGui::PushStyleColor(ImGuiCol_Text, HUD_RED);
        ImGui::Text("          GAME OVER");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, HUD_GREEN);
        ImGui::Text("Manches survécues : %d", data.combat.score);
        ImGui::Text("Zombies tués : %d", data.combat.kills);
        ImGui::Text("Temps tenu : %02d:%02d", totalSec / 60, totalSec % 60);
        ImGui::Text("A / O : retour au menu");
        ImGui::PopStyleColor();
        ImGui::End();
    }
}

}  /* namespace artouste::ui */
