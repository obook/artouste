/*
 * Hud.cpp
 * Affichage tête haute avec Dear ImGui : cycle de vie (init/shutdown), aiguillage
 * d'une image selon le mode (render), panneaux des quatre coins et bandeaux
 * centraux (pause, confirmations). Le mode superposé, les étiquettes des lieux et
 * la minimap sont dans HudOverlay.cpp ; les utilitaires de dessin dans HudWidgets.hpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "ui/Hud.hpp"

#include "physics/constants.hpp"
#include "ui/HudWidgets.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <cmath>

namespace artouste::ui {

using namespace hud_widgets;

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

void Hud::render(const HudData& data, HudMode mode, bool paused, bool confirmReset,
                 bool confirmDemo, bool forceLabels) {
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

    if (mode == HudMode::Corners) {
        renderCorners(data, w, h, m);
    } else if (mode == HudMode::Overlay) {
        renderOverlay(data, w, h, m);
    }
    /* mode Off : aucun affichage de vol. */

    /* Aide à l'atterrissage : réticule et score dessinés par-dessus tous les modes,
       y compris en démo (HUD éteint). La méthode se contente de ne rien faire si le
       guidage n'est pas actif. */
    renderPadGuidance(data, w, h);

    /* Étiquettes des lieux : affichées avec le HUD, et aussi quand on les force (démo
       HUD éteint). La minimap, elle, ne s'affiche qu'avec le HUD. */
    const bool showLabels = (mode != HudMode::Off) || forceLabels;
    if (showLabels) {
        renderLabels(data, w, h);
    }
    if (mode != HudMode::Off) {
        renderMinimap(data, mode, m);
    }

    renderBanners(paused, confirmReset, confirmDemo, w, h);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Hud::renderCorners(const HudData& data, float w, float h, float m) {
    /* Même vert instrument que le Super HUD, pour unifier les deux affichages.
     * Les textes d'alerte (TextColored) gardent leur couleur propre. */
    ImGui::PushStyleColor(ImGuiCol_Text, HUD_GREEN);

    corner("hud_tl", ImVec2(m, m), ImVec2(0.0f, 0.0f));
    ImGui::Text("ALT  %5.0f m", static_cast<double>(data.altitudeM));
    ImGui::Text("V/S  %+5.0f ft/min", static_cast<double>(data.varioFpm));
    ImGui::End();

    corner("hud_tr", ImVec2(w - m, m), ImVec2(1.0f, 0.0f));
    ImGui::Text("IAS  %4.0f kt", static_cast<double>(data.airspeedKt));
    ImGui::Text("HDG  %03.0f", static_cast<double>(data.headingDeg));
    /* Heure du simulateur : HH:MM, le deux-points clignote à 1 Hz (police à chasse
       fixe, donc l'espace garde l'alignement). En temps réel (échelle 1) on
       n'affiche rien de plus ; sinon on rappelle le facteur (ex. "x144"). */
    {
        const int  totalSec = static_cast<int>(data.timeOfDaySec) % 86400;
        const int  hh       = totalSec / 3600;
        const int  mm       = (totalSec % 3600) / 60;
        const char sep      = data.colonOn ? ':' : ' ';
        if (data.timeScale == 1.0f) {
            ImGui::Text("HRE  %02d%c%02d", hh, sep, mm);
        } else {
            ImGui::Text("HRE  %02d%c%02d  x%g", hh, sep, mm,
                        static_cast<double>(data.timeScale));
        }
    }
    if (data.geoValid) {
        ImGui::Text("LAT  %.4f %c", static_cast<double>(std::fabs(data.latDeg)),
                    data.latDeg >= 0.0f ? 'N' : 'S');
        ImGui::Text("LON  %.4f %c", static_cast<double>(std::fabs(data.lonDeg)),
                    data.lonDeg >= 0.0f ? 'E' : 'W');
    }
    ImGui::End();

    corner("hud_bl", ImVec2(m, h - m), ImVec2(0.0f, 1.0f));
    ImGui::Text("TURB %s", data.turbine);
    ImGui::Text("NR   %3.0f %%", static_cast<double>(data.rotorPct));
    ImGui::Text("COLL %3.0f %%", static_cast<double>(data.collectivePct));
    if (data.fuelLiters < physics::FUEL_LOW_L) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "CARB %4.0f L  BAS",
                           static_cast<double>(data.fuelLiters));
    } else {
        ImGui::Text("CARB %4.0f L", static_cast<double>(data.fuelLiters));
    }
    ImGui::End();

    /* Coin bas-droit : uniquement le voyant du mode assisté, affiché quand il est
       actif (l'ancienne barre de palonnier, peu utile, a été retirée). On ne crée
       le panneau que s'il y a quelque chose à montrer, pour ne pas laisser une
       boîte vide. */
    if (data.assist || data.radio) {
        corner("hud_br", ImVec2(w - m, h - m), ImVec2(1.0f, 1.0f));
        if (data.radio) {
            ImGui::Text("RADIO %d%%", data.radioMixPct);  /* voyant radio (touche K) + balance (-/+) */
        }
        if (data.assist) {
            ImGui::TextUnformatted("MODE ASSISTE");  /* vert hérité, comme les instruments */
        }
        ImGui::End();
    }

    ImGui::PopStyleColor(1);
}

void Hud::renderBanners(bool paused, bool confirmReset, bool confirmDemo, float w, float h) {
    constexpr ImGuiWindowFlags bannerFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

    /* Les panneaux de confirmation (reset, démo) sont prioritaires sur le bandeau de pause. */
    if (confirmReset) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("confirm_reset", nullptr, bannerFlags);
        ImGui::Text("       RÉINITIALISER ?");
        ImGui::Text("Replacer l'appareil au départ");
        ImGui::Text("A / O : Oui        B / N : Non");
        ImGui::End();
    } else if (confirmDemo) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("confirm_demo", nullptr, bannerFlags);
        ImGui::Text("     LANCER LA DÉMO ?");
        ImGui::Text("Vol automatique (Dune du Pilat)");
        ImGui::Text("A / O : Oui        B / N : Non");
        ImGui::End();
    } else if (paused) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGui::Begin("pause", nullptr, bannerFlags);
        ImGui::Text("        PAUSE");
        ImGui::Text("P : reprendre    Échap : quitter");
        ImGui::End();
    }
}

}  /* namespace artouste::ui */
