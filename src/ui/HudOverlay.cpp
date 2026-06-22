/*
 * HudOverlay.cpp
 * Affichages superposés du HUD : le mode "Super HUD" (rubans de cap et d'altitude,
 * rang de cadrans ronds, voyants d'alerte), les étiquettes des lieux remarquables
 * projetées sur la scène, et la minimap (orthophoto, points et marqueur appareil).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "ui/Hud.hpp"

#include "physics/constants.hpp"
#include "ui/HudWidgets.hpp"

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
    headingTape(dl, w * 0.5f, 30.0f, 230.0f, 40.0f, data.headingDeg);

    /* Ruban d'altitude vertical, à gauche de l'image. */
    altitudeTape(dl, 24.0f, h * 0.5f, 60.0f, h * 0.28f, data.altitudeM);

    /* Valeurs pré-formatées (formats littéraux : pas de format dynamique). */
    char nr[16], turb[16], ias[16], vs[16], coll[16], tmp[16], fuel[16];
    std::snprintf(nr,   sizeof(nr),   "%.0f",  static_cast<double>(data.rotorRpm));
    std::snprintf(turb, sizeof(turb), "%.0f",  static_cast<double>(data.turbineRpm));
    std::snprintf(ias,  sizeof(ias),  "%.0f",  static_cast<double>(data.airspeedKt));
    std::snprintf(vs,   sizeof(vs),   "%+.1f", static_cast<double>(data.varioMs));
    std::snprintf(coll, sizeof(coll), "%.0f",  static_cast<double>(data.collectivePct));
    std::snprintf(tmp,  sizeof(tmp),  "%.0f",  static_cast<double>(data.exhaustTempC));
    std::snprintf(fuel, sizeof(fuel), "%.0f",  static_cast<double>(data.fuelLiters));

    struct G {
        float       value, vmin, vmax, bandMin, bandMax;
        const char* label;
        const char* text;
    };
    const G gauges[] = {
        {data.rotorRpm,      0.0f, 420.0f,   340.0f,   380.0f,   "NR tr/min", nr},
        {data.turbineRpm,    0.0f, 35000.0f, 33000.0f, 34000.0f, "TURBINE",   turb},
        {data.airspeedKt,    0.0f, 140.0f,   95.0f,    105.0f,   "IAS kt",    ias},
        {data.varioMs,     -15.0f, 15.0f,    0.0f,     0.0f,     "V/S m/s",   vs},
        {data.collectivePct, 0.0f, 100.0f,   0.0f,     0.0f,     "COLL %",    coll},
        {data.exhaustTempC,  0.0f, 550.0f,   400.0f,   480.0f,   "TMP C",     tmp},
        {data.fuelLiters,    0.0f, physics::FUEL_CAPACITY_L, physics::FUEL_LOW_L,
         physics::FUEL_CAPACITY_L, "CARB L", fuel},
    };
    const int   n  = static_cast<int>(sizeof(gauges) / sizeof(gauges[0]));
    const float r  = 38.0f;
    const float dx = 98.0f;
    const float x0 = w * 0.5f - dx * static_cast<float>(n - 1) * 0.5f;
    const float y  = h - 70.0f;
    for (int i = 0; i < n; ++i) {
        gauge(dl, x0 + dx * static_cast<float>(i), y, r, gauges[i].value, gauges[i].vmin,
              gauges[i].vmax, gauges[i].bandMin, gauges[i].bandMax, gauges[i].label,
              gauges[i].text);
    }

    /* Voyants d'alerte, empilés au-dessus du rang d'instruments (du plus bas au
     * plus haut). Orange = surveiller, rouge = limite franchie. */
    struct Warn { bool on; ImU32 col; const char* text; };
    const ImU32 orange = IM_COL32(255, 170, 40, 255);
    const ImU32 red    = IM_COL32(255, 70, 70, 255);
    const Warn  warns[] = {
        {data.exhaustTempC > physics::EXHAUST_TEMP_WARN_C
             && data.exhaustTempC <= physics::EXHAUST_TEMP_MAXI_C, orange, "TEMPERATURE"},
        {data.exhaustTempC > physics::EXHAUST_TEMP_MAXI_C, red, "TEMPERATURE MAXI"},
        {data.fuelLiters < physics::FUEL_LOW_L, red, "CARBURANT BAS"},
    };
    float wy = y - r - 26.0f;
    for (const Warn& wn : warns) {
        if (!wn.on) {
            continue;
        }
        dl->AddText(ImVec2(w * 0.5f - ImGui::CalcTextSize(wn.text).x * 0.5f, wy), wn.col, wn.text);
        wy -= 20.0f;
    }

    /* Repère du mode assisté, en bas à gauche, dans le même style que les
     * instruments : panneau gris semi-transparent et texte vert. */
    if (data.assist) {
        const char*  txt = "MODE ASSISTE";
        const ImVec2 ts  = ImGui::CalcTextSize(txt);
        const ImVec2 tp(m, h - 26.0f);
        panelRect(dl, ImVec2(tp.x - 6.0f, tp.y - 4.0f),
                  ImVec2(tp.x + ts.x + 6.0f, tp.y + ts.y + 4.0f), 4.0f);
        dl->AddText(tp, HUD_GREEN, txt);
    }
}

void Hud::renderLabels(const HudData& data, float w, float h) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    /* Étiquettes projetées sur la scène : un point jaune et le nom (avec ombre).
       Anti-chevauchement : on traite les lieux du plus proche au plus lointain
       et on masque le nom de ceux dont le texte recouvrirait une étiquette déjà
       posée. Le point jaune, lui, reste affiché pour tous : seule la cohue de
       noms est évitée, pas le repérage des positions. */
    std::vector<int> order;
    order.reserve(data.labels.size());
    for (int i = 0; i < static_cast<int>(data.labels.size()); ++i) {
        if (data.labels[static_cast<std::size_t>(i)].onScreen) {
            order.push_back(i);
        }
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return data.labels[static_cast<std::size_t>(a)].depth
             < data.labels[static_cast<std::size_t>(b)].depth;
    });

    std::vector<ImVec4> placed;  /* boites de noms déjà occupées : (minx, miny, maxx, maxy) */
    placed.reserve(order.size());
    for (const int idx : order) {
        const HudLabel& lab = data.labels[static_cast<std::size_t>(idx)];
        const float     x   = lab.fx * w;
        const float     y   = lab.fy * h;
        dl->AddCircleFilled(ImVec2(x, y), 3.0f, IM_COL32(255, 230, 90, 230));

        const ImVec2 ts = ImGui::CalcTextSize(lab.name);
        const ImVec2 tp(x - ts.x * 0.5f, y - ts.y - 7.0f);
        /* Boite du nom, élargie d'une petite marge pour aérer les étiquettes. */
        const ImVec4 box(tp.x - 2.0f, tp.y - 1.0f, tp.x + ts.x + 2.0f, tp.y + ts.y + 1.0f);
        bool overlaps = false;
        for (const ImVec4& p : placed) {
            if (box.x < p.z && box.z > p.x && box.y < p.w && box.w > p.y) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) {
            continue;  /* nom masqué (le point reste) pour garder l'affichage lisible */
        }
        placed.push_back(box);
        dl->AddText(ImVec2(tp.x + 1.0f, tp.y + 1.0f), IM_COL32(0, 0, 0, 200), lab.name);
        dl->AddText(tp, IM_COL32(255, 240, 140, 255), lab.name);
    }
}

void Hud::renderMinimap(const HudData& data, HudMode mode, float m) {
    /* Minimap : orthophoto (nord en haut), points remarquables et appareil. En mode
       coins, sous le panneau d'altitude (coin haut-gauche) ; en mode superposé, calée
       tout en haut et un peu plus petite pour passer au-dessus du ruban d'altitude
       vertical (qui occupe le bord gauche). */
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (data.mapTexId == 0) {
        return;
    }
    const bool   overlay = (mode == HudMode::Overlay);
    const float  sz = overlay ? 136.0f : 150.0f;
    const ImVec2 p0(m, overlay ? m : m + 56.0f);
    const ImVec2 p1(p0.x + sz, p0.y + sz);
    dl->AddRectFilled(ImVec2(p0.x - 2.0f, p0.y - 2.0f), ImVec2(p1.x + 2.0f, p1.y + 2.0f),
                      IM_COL32(0, 0, 0, 120));
    /* L'orthophoto est chargée retournée verticalement : nord en haut -> uv (0,1)-(1,0). */
    dl->AddImage(static_cast<ImTextureID>(data.mapTexId), p0, p1, ImVec2(0.0f, 1.0f),
                 ImVec2(1.0f, 0.0f));
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 160));
    for (const HudLabel& lab : data.labels) {
        const ImVec2 q(p0.x + lab.mapU * sz, p0.y + lab.mapV * sz);
        dl->AddCircleFilled(q, 2.5f, IM_COL32(255, 230, 90, 255));
        dl->AddCircle(q, 2.5f, IM_COL32(0, 0, 0, 160));
    }
    /* Marqueur de l'appareil : triangle orienté selon le cap (nord en haut). */
    const ImVec2 c(p0.x + data.mapHeliU * sz, p0.y + data.mapHeliV * sz);
    const float  a = data.mapHeadingDeg * 3.14159265f / 180.0f;
    const ImVec2 fwd(std::sin(a), -std::cos(a));
    const ImVec2 rgt(-fwd.y, fwd.x);
    const ImVec2 tip(c.x + fwd.x * 7.0f, c.y + fwd.y * 7.0f);
    const ImVec2 bl(c.x - fwd.x * 4.0f + rgt.x * 4.0f, c.y - fwd.y * 4.0f + rgt.y * 4.0f);
    const ImVec2 br(c.x - fwd.x * 4.0f - rgt.x * 4.0f, c.y - fwd.y * 4.0f - rgt.y * 4.0f);
    dl->AddTriangleFilled(tip, bl, br, IM_COL32(255, 70, 70, 255));
    dl->AddTriangle(tip, bl, br, IM_COL32(0, 0, 0, 180));
}

}  /* namespace artouste::ui */
