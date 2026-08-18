/*
 * HudAlerts.cpp
 * Alertes de vol par-dessus tous les modes de HUD : vortex ring state, vitesse
 * au-delà de la VNE et taux de descente (façon GPWS). Toutes clignotent au même
 * rythme que les LED d'alarme (voir HudData::alarmBlinkOn). Le reste des
 * bandeaux communs est dans ui/Hud.cpp.
 *
 * Leurs hauteurs sont fixées et espacées pour qu'un piqué rapide près du sol,
 * qui allume vitesse ET taux de descente, ne les superpose pas : vortex à 0,30,
 * vitesse à 0,35, taux de descente à 0,40, zone H-V à 0,47.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "ui/Hud.hpp"
#include "ui/HudAlarms.hpp"
#include "ui/HudWidgets.hpp"

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
    constexpr ImGuiWindowFlags flags = hud_widgets::HUD_FLAGS;

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

void Hud::renderVitesseAlert(const HudData& data, float w, float h) {
    /* Rouge seulement, c'est-à-dire VNE franchie : le jaune du cadran IAS est un
       préavis à 95 %, il reste à la LED. Même source que ce cadran, donc rien de
       plus à porter dans HudData. */
    if (hud_widgets::alarmeIas(data) != hud_widgets::GaugeLed::Red) {
        return;
    }
    /* Clignotement, comme le vortex et les LED d'alarme (~2 Hz). */
    if (!data.alarmBlinkOn) {
        return;
    }
    constexpr ImGuiWindowFlags flags = hud_widgets::HUD_FLAGS;

    /* Entre le vortex et le taux de descente. Le vortex demande une vitesse
       faible, les deux ne peuvent donc pas s'allumer ensemble ; le taux de
       descente, si, d'où la place réservée juste au-dessus de lui. */
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.35f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("vitesse_alert", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.20f, 1.0f)); /* rouge alarme */
    ImGui::Text("VITESSE EXCESSIVE");
    ImGui::PopStyleColor();
    ImGui::End();
}

void Hud::renderHvAlert(const HudData& data, float w, float h) {
    /* Indicateur discret, à la différence des bandeaux rouges : la zone à éviter
       du diagramme hauteur-vitesse n'est pas une urgence mais une exposition (en
       cas de panne, l'autorotation n'est pas garantie). Texte ambre fixe, sans
       clignotement, seuil au-delà du lissage de FlightModel (~1 s dans la zone). */
    if (data.hvIntensity < 0.5f) {
        return;
    }
    constexpr ImGuiWindowFlags flags = hud_widgets::HUD_FLAGS;

    /* Sous les bandeaux vortex et taux de descente, pour pouvoir coexister. */
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.47f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("hv_alert", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.20f, 1.0f)); /* ambre */
    ImGui::Text("ZONE H-V");
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
    constexpr ImGuiWindowFlags flags = hud_widgets::HUD_FLAGS;

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
