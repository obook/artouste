/*
 * HudCorners.cpp
 * Mode HUD "quatre coins" : altitude/vario, vitesse/cap/heure, turbine/rotor/
 * carburant, et voyants (assisté, atterrissage automatique, radio, images par
 * seconde). Le mode superposé (Super HUD) est dans HudSuperOverlay.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "physics/constants.hpp"
#include "ui/Hud.hpp"
#include "ui/HudAlarms.hpp"
#include "ui/HudWidgets.hpp"

#include <imgui.h>

#include <cmath>
#include <cstdarg>

namespace artouste::ui {

using namespace hud_widgets;

namespace {

/* Ligne de texte du HUD 4 coins colorée selon un état d'alarme : vert instrument
   hérité quand tout est normal (ou alarme inhibée), jaune ou rouge sinon. Mêmes
   états que les LED des cadrans du Super HUD (voir HudAlarms.hpp) : les deux
   affichages signalent la même chose, chacun avec ses moyens. */
void ligneAlarme(GaugeLed etat, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (etat == GaugeLed::Yellow || etat == GaugeLed::Red) {
        ImGui::PushStyleColor(ImGuiCol_Text, (etat == GaugeLed::Red) ? HUD_RED : HUD_AMBER);
        ImGui::TextV(fmt, args);
        ImGui::PopStyleColor();
    } else {
        ImGui::TextV(fmt, args);
    }
    va_end(args);
}

} /* namespace */

void Hud::renderCorners(const HudData& data, float w, float h, float m) {
    /* Même vert instrument que le Super HUD, pour unifier les deux affichages.
     * Les lignes surveillées (ligneAlarme) passent au jaune ou au rouge. */
    ImGui::PushStyleColor(ImGuiCol_Text, HUD_GREEN);

    corner("hud_tl", ImVec2(m, m), ImVec2(0.0f, 0.0f));
    ImGui::Text("ALT  %5.0f m", static_cast<double>(data.altitudeM));
    ImGui::Text("V/S  %+5.1f m/s", static_cast<double>(data.varioMs));
    ImGui::End();

    corner("hud_tr", ImVec2(w - m, m), ImVec2(1.0f, 0.0f));
    ligneAlarme(alarmeIas(data), "IAS  %4.0f km/h", static_cast<double>(data.airspeedKmh));
    ImGui::Text("HDG  %03.0f", static_cast<double>(data.headingDeg));
    /* Heure du simulateur : HH:MM, le deux-points clignote à 1 Hz (police à chasse
       fixe, donc l'espace garde l'alignement). En temps réel (échelle 1) on
       n'affiche rien de plus ; sinon on rappelle le facteur (ex. "x144"). */
    {
        const int totalSec = static_cast<int>(data.timeOfDaySec) % 86400;
        const int hh = totalSec / 3600;
        const int mm = (totalSec % 3600) / 60;
        const char sep = data.colonOn ? ':' : ' ';
        if (data.timeScale == 1.0f) {
            ImGui::Text("HRE  %02d%c%02d", hh, sep, mm);
        } else {
            ImGui::Text("HRE  %02d%c%02d  x%g", hh, sep, mm, static_cast<double>(data.timeScale));
        }
    }
    if (data.geoValid) {
        ImGui::Text("LAT  %.4f %c",
                    static_cast<double>(std::fabs(data.latDeg)),
                    data.latDeg >= 0.0f ? 'N' : 'S');
        ImGui::Text("LON  %.4f %c",
                    static_cast<double>(std::fabs(data.lonDeg)),
                    data.lonDeg >= 0.0f ? 'E' : 'W');
    }
    ImGui::End();

    corner("hud_bl", ImVec2(m, h - m), ImVec2(0.0f, 1.0f));
    ligneAlarme(alarmeTurbine(data), "TURB %s", data.turbine);
    ligneAlarme(alarmeNr(data), "NR   %3.0f %%", static_cast<double>(data.rotorPct));
    ImGui::Text("COLL %3.0f %%", static_cast<double>(data.collectivePct));
    ligneAlarme(alarmeTmp(data), "TMP  %3.0f C", static_cast<double>(data.exhaustTempC));
    const GaugeLed alCarb = alarmeCarb(data);
    if (data.fuelLiters <= 0.0f) {
        /* Réservoir vide : ce n'est plus un niveau bas mais la panne sèche, et
           c'est elle qui explique la turbine arrêtée et le rotor qui descend. */
        ligneAlarme(alCarb, "CARB %4.0f L  PANNE", static_cast<double>(data.fuelLiters));
    } else if (alCarb == GaugeLed::Red) {
        ligneAlarme(alCarb, "CARB %4.0f L  BAS", static_cast<double>(data.fuelLiters));
    } else {
        ligneAlarme(alCarb, "CARB %4.0f L", static_cast<double>(data.fuelLiters));
    }
    ImGui::End();

    /* Coin bas-droit : voyants du mode assisté, de l'atterrissage automatique et de la
       radio quand ils sont actifs, et le compteur d'images par seconde (dernière ligne,
       donc pile dans le coin).
       On ne crée le panneau que s'il y a quelque chose à montrer, pour ne pas laisser
       une boîte vide (fps = 0 en capture => rien). */
    if (data.assist || data.autoland || data.radio || data.fps > 0.0f) {
        corner("hud_br", ImVec2(w - m, h - m), ImVec2(1.0f, 1.0f));
        if (data.radio) {
            ImGui::Text("RADIO %d%%",
                        data.radioMixPct); /* voyant radio (touche K) + balance (-/+) */
        }
        if (data.assist) {
            ImGui::TextUnformatted("MODE ASSISTE"); /* vert hérité, comme les instruments */
        }
        if (data.autoland) {
            ImGui::TextUnformatted("ATTERRISSAGE AUTO"); /* touche J / RB */
        }
        if (data.fps > 0.0f) {
            ImGui::Text("FPS  %3.0f", static_cast<double>(data.fps)); /* cadence lissée */
        }
        ImGui::End();
    }

    ImGui::PopStyleColor(1);
}

} /* namespace artouste::ui */
