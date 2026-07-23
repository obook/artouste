/*
 * HudAlerts.cpp
 * Alertes de vol par-dessus tous les modes de HUD : vortex ring state et taux
 * de descente (façon GPWS). Toutes deux clignotent au même rythme que les LED
 * d'alarme (voir HudData::alarmBlinkOn). Le reste des bandeaux communs est
 * dans ui/Hud.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "ui/Hud.hpp"

#include <imgui.h>

namespace artouste::ui {

void Hud::renderVortexAlert(const HudData& data, float w, float h) {
    /* Seuil d'affichage : on n'alerte qu'une fois le phénomène franchement installé,
       pour éviter un clignotement au moindre effleurement. */
    constexpr float VORTEX_ALERT_MIN = 0.15f;
    if (data.vrsIntensity < VORTEX_ALERT_MIN) {
        return;
    }
    /* Clignotement : bandeau affiché seulement pendant la phase allumée du battement
       des alarmes (~2 Hz), pour attirer l'oeil comme les LED d'alarme. En capture,
       alarmBlinkOn est figé à true (bandeau visible). */
    if (!data.alarmBlinkOn) {
        return;
    }
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

    /* Tiers supérieur, centré : au-dessus de l'appareil et du réticule d'aide au posé,
       sous le ruban de cap du HUD complet, et distinct du sous-titre radio (en haut). */
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.30f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("vortex_alert", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.20f, 1.0f)); /* rouge alarme */
    ImGui::Text("         VORTEX");
    ImGui::Text("REPRENDRE DE LA VITESSE");
    ImGui::PopStyleColor();
    ImGui::End();
}

void Hud::renderSinkRateAlert(const HudData& data, float w, float h) {
    if (!data.sinkRateAlert) {
        return;
    }
    /* Clignotement, comme le vortex et les LED d'alarme (~2 Hz). */
    if (!data.alarmBlinkOn) {
        return;
    }
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

    /* Juste sous l'emplacement du bandeau vortex : les deux peuvent coexister (descente
       rapide a faible vitesse pres du sol) sans se recouvrir. */
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.40f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("sinkrate_alert", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.20f, 1.0f)); /* rouge alarme */
    ImGui::Text("TAUX DE DESCENTE");
    ImGui::PopStyleColor();
    ImGui::End();
}

} /* namespace artouste::ui */
