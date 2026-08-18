/*
 * HudSuperOverlay.cpp
 * Mode "Super HUD" : rubans de cap et d'altitude, rang de cadrans ronds,
 * voyants d'alerte et badges radio/assistance. Extrait de HudOverlay.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "ui/Hud.hpp"

#include "physics/constants.hpp"
#include "ui/HudAlarms.hpp"
#include "ui/HudWidgets.hpp"

#include "ui/hud/HudCadran.hpp"
#include "ui/hud/HudRubans.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace artouste::ui {

using namespace hud_widgets;

void Hud::renderOverlay(const HudData& data, float w, float h, float m) {
    /* Super HUD : rang d'instruments ronds verts superposés en bas de l'image
     * (Priorité 1 de PANEL.md), assez bas pour ne pas gêner la vue de vol. */
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    /* Ruban de cap qui défile, en haut de l'image. */
    headingTape(dl, w * 0.5f, sc(30.0f), sc(230.0f), sc(40.0f), data.headingDeg);

    /* Ruban d'altitude vertical, à droite de l'image (disposition demandée par un
       pilote réel), volontairement étroit. Le cadran V/S (voir plus bas, sur la
       ligne des instruments du bas), plus large que le ruban une fois son panneau
       et sa LED comptés, fixe la marge de droite ; le ruban se cale sur sa colonne
       et bascule son étiquette de valeur à gauche pour ne jamais déborder. */
    const float vsR          = sc(38.0f);
    const float vsCx         = w - sc(20.0f) - vsR - sc(8.0f);
    const float altTapeLeft  = vsCx - sc(22.0f);
    altitudeTape(dl, altTapeLeft, h * 0.5f, sc(44.0f), h * 0.22f, data.altitudeM, true);

    /* Valeurs pré-formatées (formats littéraux : pas de format dynamique). */
    char nr[16], turb[16], ias[16], vs[16], coll[16], tmp[16], fuel[16];
    std::snprintf(nr,   sizeof(nr),   "%.0f",  static_cast<double>(data.rotorRpm));
    std::snprintf(turb, sizeof(turb), "%.0f",  static_cast<double>(data.turbineRpm));
    std::snprintf(ias,  sizeof(ias),  "%.0f",  static_cast<double>(data.airspeedKmh));
    std::snprintf(vs,   sizeof(vs),   "%+.1f", static_cast<double>(data.varioMs));
    std::snprintf(coll, sizeof(coll), "%.1f",  static_cast<double>(data.pasDeg));
    std::snprintf(tmp,  sizeof(tmp),  "%.0f",  static_cast<double>(data.exhaustTempC));
    std::snprintf(fuel, sizeof(fuel), "%.0f",  static_cast<double>(data.fuelLiters));

    /* LED d'alarme de quatre cadrans, en haut à droite dans l'instrument. Les états
       (vert, jaune, rouge) sont partagés avec les lignes du HUD 4 coins, voir
       HudAlarms.hpp pour les seuils. */
    const GaugeLed ledNr   = alarmeNr(data);
    const GaugeLed ledTurb = alarmeTurbine(data);
    const GaugeLed ledIas  = alarmeIas(data);
    const GaugeLed ledTmp  = alarmeTmp(data);
    const GaugeLed ledCarb = alarmeCarb(data);
    const GaugeLed ledVs   = alarmeVario(data);

    /* Alerte VRS (vortex ring state) : taux de descente excessif à faible vitesse
       sol. Une descente rapide en translation (approche normale à forte pente) ne
       doit pas déclencher l'alarme, d'où la condition de vitesse. Reprend les
       seuils VRS_DESCENT_MIN/MAX du modèle physique (voir FlightModel.cpp, qui les
       utilise pour la perte de portance et le bandeau plein écran vrsIntensity) :
       ce voyant, lui, avertit plus tôt et reste localisé au cadran V/S. varioMs est
       négatif en descente, d'où le signe inversé dans les comparaisons. */
    constexpr float KT_PER_KMH   = 1.0f / 1.852f;
    constexpr float VRS_EXIT_KT  = physics::VRS_AIRSPEED_EXIT * 3.6f * KT_PER_KMH;  /* ~14 kt */
    const float     airspeedKt   = data.airspeedKmh * KT_PER_KMH;
    const bool      vrsApproche  = (data.varioMs < -physics::VRS_DESCENT_MIN)
                               && (airspeedKt < VRS_EXIT_KT);
    const bool      vrsDeveloppe = (data.varioMs < -physics::VRS_DESCENT_MAX)
                               && (airspeedKt < VRS_EXIT_KT);

    struct G {
        float       value, vmin, vmax, bandMin, bandMax;
        const char* label;
        const char* text;
        GaugeLed    led;
    };
    const float r  = sc(38.0f);
    const float dx = sc(98.0f);

    /* Groupe central, réduit à la chaîne mécanique et aux limites moteur (disposition
       demandée par un pilote réel : Turbine, NR, TMP, CARB seuls, le reste ayant migré
       ailleurs, voir plus bas). La turbine précède le NR : la chaîne mécanique
       (turbine -> roue libre -> rotor) et la séquence de démarrage se lisent ainsi de
       gauche à droite. */
    const G centre[] = {
        {data.turbineRpm,   0.0f, 35000.0f, 33000.0f, 34000.0f, "TURBINE",   turb, ledTurb},
        {data.rotorRpm,     0.0f, 420.0f,   340.0f,   380.0f,   "NR tr/min", nr,   ledNr},
        {data.exhaustTempC, 0.0f, 550.0f,   400.0f,   480.0f,   "TMP C",     tmp,  ledTmp},
        {data.fuelLiters,   0.0f, physics::FUEL_CAPACITY_L, physics::FUEL_LOW_L,
         physics::FUEL_CAPACITY_L, "CARB L", fuel, ledCarb},
    };
    const int   nCentre = static_cast<int>(sizeof(centre) / sizeof(centre[0]));
    const float xCentre = w * 0.5f - dx * static_cast<float>(nCentre - 1) * 0.5f;
    const float y       = h - sc(70.0f);
    for (int i = 0; i < nCentre; ++i) {
        gauge(dl, xCentre + dx * static_cast<float>(i), y, r, centre[i].value, centre[i].vmin,
              centre[i].vmax, centre[i].bandMin, centre[i].bandMax, centre[i].label,
              centre[i].text, centre[i].led, data.alarmBlinkOn);
    }

    /* Collectif et IAS, en bas à gauche (disposition demandée par un pilote réel) :
       même hauteur de rangée que le groupe central, décalés vers le coin pour
       rester lisibles d'un même coup d'oeil, au-dessus des badges radio/assisté
       qui s'empilent depuis ce même coin (voir plus bas). */
    /* Cadran du pas collectif gradué en degrés de pale (manuel : plage de vol
       12-15, butée élastique à 14,5, secours à 15) : la bande verte couvre la
       plage normale, comme le cadran réel. */
    gauge(dl, m + r, y, r, data.pasDeg, physics::PAS_MIN_DEG, physics::PAS_MAX_DEG,
          12.0f, physics::PAS_BUTEE_HAUTE_DEG, "PAS deg", coll,
          GaugeLed::None, data.alarmBlinkOn);
    /* Bande du cadran IAS : elle suit la VNE DU MOMENT, qui décroît avec l'altitude,
       et non une plage figée. Elle commence là où le voyant passe au jaune (95 % de
       la VNE, voir alarmeIas) et finit à la VNE. Figée à 176-195 km/h, elle valait
       pour le seul niveau de la mer : à 950 m elle mentait de 9 km/h et l'aiguille
       n'entrait dans la bande que bien après l'allumage du voyant. Deux instruments
       qui racontent la même chose ne doivent pas se contredire. */
    const float vneKmh = physics::vneAtAltitudeMs(data.altitudeM) * 3.6f;
    gauge(dl, m + r + dx, y, r, data.airspeedKmh, 0.0f, 260.0f, 0.95f * vneKmh, vneKmh,
          "IAS km/h", ias, ledIas, data.alarmBlinkOn);

    /* Vario, sur la même ligne que les instruments du bas plutôt que sous le ruban
       d'altitude : midAngleDeg=180 pour un zéro à l'horizontale plutôt qu'en haut,
       comme le vrai VSI (demande d'un pilote réel -- l'aiguille part de 9 heures,
       monte vers midi). */
    gauge(dl, vsCx, y, vsR, data.varioMs, -15.0f, 15.0f, 0.0f, 0.0f, "V/S m/s", vs, ledVs,
          data.alarmBlinkOn, 180.0f, /*zeroLine=*/true);

    /* Voyants d'alerte, empilés au-dessus du rang d'instruments (du plus bas au
     * plus haut). Orange = surveiller, rouge = limite franchie. */
    struct Warn { bool on; ImU32 col; const char* text; };
    const Warn warns[] = {
        /* Les états jaunes des cadrans sont d'ordinaire portés par leur seule LED ;
           le texte central ne crie que les limites franchies (rouge). Le VRS fait
           exception : contrairement à une température ou un carburant bas, il se
           développe en quelques secondes, d'où un avertissement textuel dès la
           phase d'approche (orange) et pas seulement une fois développé (rouge). */
        {ledTmp == GaugeLed::Red, HUD_RED, "TEMPÉRATURE MAXI"},
        {ledCarb == GaugeLed::Red, HUD_RED, "CARBURANT BAS"},
        {vrsApproche && !vrsDeveloppe, HUD_AMBER, "TAUX DE DESCENTE"},
        {vrsDeveloppe, HUD_RED, "VORTEX"},
    };
    float wy = y - r - sc(38.0f);  /* au-dessus du panneau des cadrans (haut à y - r - 18) */
    for (const Warn& wn : warns) {
        if (!wn.on) {
            continue;
        }
        dl->AddText(ImVec2(w * 0.5f - ImGui::CalcTextSize(wn.text).x * 0.5f, wy), wn.col, wn.text);
        wy -= sc(20.0f);
    }

    /* Voyant de la radio et repère du mode assisté, en bas à gauche, dans le même
     * style que les instruments : panneau gris semi-transparent et texte vert. Le
     * voyant radio se place une ligne au-dessus du mode assisté. Empilement calculé
     * sur la hauteur réelle du texte (et non un écart fixe) : un écart fixe plus
     * petit que la hauteur d'un carton fait chevaucher les deux panneaux. Départ
     * au-dessus des cadrans collectif/IAS (voir plus haut), qui occupent maintenant
     * ce même coin jusqu'au bord de l'écran : sans ce décalage, les badges se
     * superposeraient à leur valeur affichée. */
    float nextBottom = y - r - sc(28.0f);
    const auto badge = [&](const char* txt) {
        const ImVec2 ts = ImGui::CalcTextSize(txt);
        const ImVec2 tp(m, nextBottom - sc(4.0f) - ts.y);
        panelRect(dl, ImVec2(tp.x - sc(6.0f), tp.y - sc(4.0f)),
                  ImVec2(tp.x + ts.x + sc(6.0f), tp.y + ts.y + sc(4.0f)), sc(4.0f));
        dl->AddText(tp, HUD_GREEN, txt);
        nextBottom = tp.y - sc(8.0f);  /* dessus du carton + interstice avant le suivant */
    };
    if (data.assist) {
        badge("MODE ASSISTE");
    }
    if (data.autoland) {
        badge("ATTERRISSAGE AUTO");
    }
    if (data.radio) {
        char txt[24];
        std::snprintf(txt, sizeof(txt), "RADIO %d%%", data.radioMixPct);
        badge(txt);
    }
}

}  /* namespace artouste::ui */
