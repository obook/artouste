/*
 * HudLabelsMinimap.cpp
 * Étiquettes des lieux remarquables projetées sur la scène, et minimap
 * (orthophoto, points et marqueur appareil). Extrait de HudOverlay.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "ui/Hud.hpp"

#include "physics/constants.hpp"
#include "ui/HudAlarms.hpp"
#include "ui/HudWidgets.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace artouste::ui {

using namespace hud_widgets;

/*
 * labelPointColor
 * Couleur du point d'une étiquette (scène 3D ou minimap) : celle de la balise HAPI
 * du pad si elle en a une (vert/rouge, potentiellement éteinte le temps de la phase
 * de clignotement), sinon la couleur générique donnée par l'appelant (cyan pour un
 * hélipad, doré pour un lieu remarquable).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 */
static ImU32 labelPointColor(const HudLabel& lab, ImU32 defaultColor) {
    if (!lab.hasHapi) {
        return defaultColor;
    }
    if (lab.hapiOff) {
        return IM_COL32(0, 0, 0, 0);
    }
    return lab.hapiGreen ? HUD_HAPI_GREEN : HUD_RED;
}

void Hud::renderLabels(const HudData& data, float w, float h) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    /* Étiquettes projetées sur la scène : un point jaune et le nom (avec ombre).
       Anti-chevauchement : on traite d'abord les lieux nommés, puis les étiquettes
       génériques ("Hélisurface"), chaque groupe du plus proche au plus lointain,
       et on masque le nom de ceux dont le texte recouvrirait une étiquette déjà
       posée. Ainsi un pad posé sur un lieu nommé ne vole pas son nom (pic du Midi
       d'Ossau). Le point jaune, lui, reste affiché pour tous : seule la cohue de
       noms est évitée, pas le repérage des positions. */
    std::vector<int> order;
    order.reserve(data.labels.size());
    for (int i = 0; i < static_cast<int>(data.labels.size()); ++i) {
        if (data.labels[static_cast<std::size_t>(i)].onScreen) {
            order.push_back(i);
        }
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const HudLabel& la = data.labels[static_cast<std::size_t>(a)];
        const HudLabel& lb = data.labels[static_cast<std::size_t>(b)];
        if (la.generic != lb.generic) {
            return !la.generic;  /* les lieux nommés d'abord */
        }
        return la.depth < lb.depth;
    });

    std::vector<ImVec4> placed;  /* boîtes de noms déjà occupées : (minx, miny, maxx, maxy) */
    placed.reserve(order.size());
    for (const int idx : order) {
        const HudLabel& lab = data.labels[static_cast<std::size_t>(idx)];
        const float     x   = lab.fx * w;
        const float     y   = lab.fy * h;
        /* Cyan pour un hélipad, jaune doré pour un lieu remarquable : le pad se
           repère au premier coup d'œil, sans se confondre avec les lieux. */
        const ImU32 couleurPoint =
            labelPointColor(lab, lab.generic ? HUD_CYAN : IM_COL32(255, 230, 90, 230));
        dl->AddCircleFilled(ImVec2(x, y), sc(3.0f), couleurPoint);

        /* Le libellé tient sur deux lignes ("nom\naltitude distance"). CalcTextSize
           gère le multi-ligne : largeur = ligne la plus large, hauteur = les deux
           lignes. La boîte et le test de chevauchement s'appuient dessus. */
        const ImVec2 ts = ImGui::CalcTextSize(lab.name.c_str());
        const ImVec2 tp(x - ts.x * 0.5f, y - ts.y - sc(7.0f));
        /* Boîte du nom, élargie d'une petite marge pour aérer les étiquettes. */
        const ImVec4 box(tp.x - sc(2.0f), tp.y - sc(1.0f),
                         tp.x + ts.x + sc(2.0f), tp.y + ts.y + sc(1.0f));
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
        /* Rendu ligne à ligne : chaque ligne est recentrée sur x (AddText cale à
           gauche, il faut donc centrer soi-même), avec une ombre portée. Boucle
           générique : fonctionne aussi bien pour une que pour deux lignes. */
        const float lineH = ImGui::GetTextLineHeight();
        const char* lineStart = lab.name.c_str();
        for (int li = 0;; ++li) {
            const char* lineEnd = lineStart;
            while (*lineEnd != '\0' && *lineEnd != '\n') {
                ++lineEnd;
            }
            const ImVec2 lts = ImGui::CalcTextSize(lineStart, lineEnd);
            const float  lx  = x - lts.x * 0.5f;
            const float  ly  = tp.y + static_cast<float>(li) * lineH;
            dl->AddText(ImVec2(lx + sc(1.0f), ly + sc(1.0f)), IM_COL32(0, 0, 0, 200),
                        lineStart, lineEnd);
            dl->AddText(ImVec2(lx, ly), IM_COL32(255, 240, 140, 255), lineStart, lineEnd);
            if (*lineEnd == '\0') {
                break;
            }
            lineStart = lineEnd + 1;
        }
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
    const float  sz = overlay ? sc(136.0f) : sc(150.0f);
    const ImVec2 p0(m, overlay ? m : m + sc(56.0f));
    const ImVec2 p1(p0.x + sz, p0.y + sz);
    dl->AddRectFilled(ImVec2(p0.x - sc(2.0f), p0.y - sc(2.0f)),
                      ImVec2(p1.x + sc(2.0f), p1.y + sc(2.0f)), IM_COL32(0, 0, 0, 120));
    /* L'orthophoto est chargée retournée verticalement : nord en haut -> uv (0,1)-(1,0). */
    dl->AddImage(static_cast<ImTextureID>(data.mapTexId), p0, p1, ImVec2(0.0f, 1.0f),
                 ImVec2(1.0f, 0.0f));
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 160));
    for (const HudLabel& lab : data.labels) {
        const ImVec2 q(p0.x + lab.mapU * sz, p0.y + lab.mapV * sz);
        const ImU32  couleurPoint =
            labelPointColor(lab, lab.generic ? HUD_CYAN : IM_COL32(255, 230, 90, 255));
        dl->AddCircleFilled(q, sc(2.5f), couleurPoint);
        dl->AddCircle(q, sc(2.5f), IM_COL32(0, 0, 0, 160));
    }
    /* Mode zombie : petits points verts, distincts des lieux/hélipads (dorés/
       cyan) et du marqueur de l'appareil (rouge) -- bien visibles pour repérer
       la horde sans avoir à la chercher dans le paysage. Dessinés avant le
       marqueur de l'appareil pour que celui-ci reste au-dessus en cas de
       chevauchement. */
    for (const HudData::CombatHud::MapPoint& zp : data.combat.zombieMapPoints) {
        const ImVec2 q(p0.x + zp.u * sz, p0.y + zp.v * sz);
        dl->AddCircleFilled(q, sc(2.5f), HUD_GREEN);
        dl->AddCircle(q, sc(2.5f), IM_COL32(0, 0, 0, 160));
    }

    /* Marqueur de l'appareil : triangle orienté selon le cap (nord en haut). */
    const ImVec2 c(p0.x + data.mapHeliU * sz, p0.y + data.mapHeliV * sz);
    const float  a = data.mapHeadingDeg * 3.14159265f / 180.0f;
    const ImVec2 fwd(std::sin(a), -std::cos(a));
    const ImVec2 rgt(-fwd.y, fwd.x);
    const ImVec2 tip(c.x + fwd.x * sc(7.0f), c.y + fwd.y * sc(7.0f));
    const ImVec2 bl(c.x - fwd.x * sc(4.0f) + rgt.x * sc(4.0f),
                    c.y - fwd.y * sc(4.0f) + rgt.y * sc(4.0f));
    const ImVec2 br(c.x - fwd.x * sc(4.0f) - rgt.x * sc(4.0f),
                    c.y - fwd.y * sc(4.0f) - rgt.y * sc(4.0f));
    dl->AddTriangleFilled(tip, bl, br, IM_COL32(255, 70, 70, 255));
    dl->AddTriangle(tip, bl, br, IM_COL32(0, 0, 0, 180));
}

}  /* namespace artouste::ui */
