/*
 * HudRubans.hpp
 * Rubans de cap et d'altitude du HUD complet.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "ui/HudWidgets.hpp"

#include <imgui.h>

#include <cmath>
#include <cstdio>

namespace artouste::ui::hud_widgets {

/* Ramène un angle dans [-180, +180] degrés (plus court chemin). */
inline float wrap180(float a) {
    a = std::fmod(a + 180.0f, 360.0f);
    if (a < 0.0f) {
        a += 360.0f;
    }
    return a - 180.0f;
}

/* Ruban de cap horizontal, en haut de l'image : un rectangle avec une échelle
 * graduée qui défile sous un repère central fixe ; le cap courant est affiché
 * au-dessus du repère. Graduations tous les 5 degrés, libellé tous les 10
 * (lettres N/E/S/O aux points cardinaux, sinon les dizaines : 03, 06, 12...). */
inline void headingTape(ImDrawList* dl, float cx, float top, float halfWidth, float height,
                        float heading) {
    const float pxPerDeg = sc(4.0f);
    const float spanDeg  = halfWidth / pxPerDeg;  /* demi-plage visible, en degrés */
    const ImVec2 tl{cx - halfWidth, top};
    const ImVec2 br{cx + halfWidth, top + height};

    /* Fond du ruban : il épouse le ruban (petite marge symétrique). La valeur du cap,
       au-dessus, a son propre cadre sombre : inutile d'étendre ce fond vers le haut. */
    panelRect(dl, ImVec2(tl.x, top - sc(2.0f)), ImVec2(br.x, br.y + sc(2.0f)), sc(4.0f));
    dl->AddRect(tl, br, HUD_GREEN, 0.0f, 0, sc(1.5f));
    dl->PushClipRect(tl, br, true);

    const int start = static_cast<int>(std::floor((heading - spanDeg) / 5.0f)) * 5;
    const int end   = static_cast<int>(std::ceil((heading + spanDeg) / 5.0f)) * 5;
    for (int v = start; v <= end; v += 5) {
        const float x = cx + wrap180(static_cast<float>(v) - heading) * pxPerDeg;
        const bool  major = (v % 10) == 0;
        hudLine(dl, ImVec2(x, top), ImVec2(x, top + (major ? height * 0.5f : height * 0.3f)),
                 HUD_GREEN, sc(1.5f));
        if (major) {
            const int   hd = ((v % 360) + 360) % 360;
            char        buf[4];
            if (hd == 0) {
                std::snprintf(buf, sizeof(buf), "N");
            } else if (hd == 90) {
                std::snprintf(buf, sizeof(buf), "E");
            } else if (hd == 180) {
                std::snprintf(buf, sizeof(buf), "S");
            } else if (hd == 270) {
                std::snprintf(buf, sizeof(buf), "O");
            } else {
                std::snprintf(buf, sizeof(buf), "%02d", hd / 10);
            }
            centeredText(dl, x, top + height * 0.55f, HUD_GREEN, buf);
        }
    }
    dl->PopClipRect();

    /* Repère central fixe (triangle pointant vers le ruban) et cap courant chiffré.
     * La valeur reçoit son propre cadre gris clair, comme celle de l'altimètre. */
    dl->AddTriangleFilled(ImVec2(cx - sc(6.0f), top - sc(1.0f)),
                          ImVec2(cx + sc(6.0f), top - sc(1.0f)), ImVec2(cx, top + sc(7.0f)),
                          HUD_BRIGHT);
    const int hdg = (static_cast<int>(heading + 0.5f) % 360 + 360) % 360;
    char      hbuf[8];
    std::snprintf(hbuf, sizeof(hbuf), "%03d", hdg);
    const ImVec2 sz = ImGui::CalcTextSize(hbuf);
    const float  ty = top - sc(20.0f);
    panelRect(dl, ImVec2(cx - sz.x * 0.5f - sc(4.0f), ty - sc(2.0f)),
              ImVec2(cx + sz.x * 0.5f + sc(4.0f), ty + sz.y + sc(2.0f)), sc(3.0f));
    centeredText(dl, cx, ty, HUD_BRIGHT, hbuf);
}

/* Ruban d'altitude vertical : même principe que la boussole mais en hauteur.
 * L'échelle (en mètres) défile, l'altitude courante reste au centre sous un
 * repère, avec sa valeur chiffrée à côté du ruban (à droite par défaut, comme
 * pour un ruban posé à gauche de l'image ; labelLeft=true la bascule à gauche
 * du ruban, pour un ruban posé à droite de l'image sans déborder de l'écran).
 * Graduations tous les 10 m, libellé tous les 50 m. */
inline void altitudeTape(ImDrawList* dl, float left, float cy, float width, float halfHeight,
                         float altitude, bool labelLeft = false) {
    const float  pxPerM    = sc(2.0f);
    const int    minorStep = 10;  /* m entre graduations */
    const int    majorStep = 50;  /* m entre libellés */
    const ImVec2 tl{left, cy - halfHeight};
    const ImVec2 br{left + width, cy + halfHeight};
    /* L'échelle défilante est une aide périphérique : ses libellés sont en police
       réduite pour garder le ruban étroit. Seule l'altitude courante, encadrée à
       droite, reste en pleine taille. */
    const float  fontSz   = ImGui::GetFontSize() * 0.85f;
    const float  halfTick = fontSz * 0.5f;
    const float  halfVal  = ImGui::GetTextLineHeight() * 0.5f;

    panelRect(dl, tl, br, sc(4.0f));
    dl->AddRect(tl, br, HUD_GREEN, 0.0f, 0, sc(1.5f));
    dl->PushClipRect(tl, br, true);

    const float rangeM = halfHeight / pxPerM;  /* demi-plage visible, en mètres */
    const int   start  = static_cast<int>(std::floor((altitude - rangeM) / minorStep)) * minorStep;
    const int   end    = static_cast<int>(std::ceil((altitude + rangeM) / minorStep)) * minorStep;
    for (int v = start; v <= end; v += minorStep) {
        const float y     = cy - (static_cast<float>(v) - altitude) * pxPerM;
        const bool  major = (v % majorStep) == 0;
        hudLine(dl, ImVec2(br.x - (major ? width * 0.4f : width * 0.22f), y), ImVec2(br.x, y),
                HUD_GREEN, sc(1.5f));
        if (major && v >= 0) {
            char buf[12];
            std::snprintf(buf, sizeof(buf), "%d", v);
            dl->AddText(ImGui::GetFont(), fontSz, ImVec2(tl.x + sc(3.0f), y - halfTick),
                        HUD_GREEN, buf);
        }
    }
    dl->PopClipRect();

    /* Repère central fixe (triangle pointant vers le ruban) et altitude courante,
       du côté du ruban opposé à l'écran (à droite par défaut, à gauche si
       labelLeft, pour ne jamais déborder du bord de l'écran). */
    char abuf[12];
    std::snprintf(abuf, sizeof(abuf), "%.0f", static_cast<double>(altitude));
    const ImVec2 sz = ImGui::CalcTextSize(abuf);
    if (!labelLeft) {
        dl->AddTriangleFilled(ImVec2(br.x, cy - sc(7.0f)), ImVec2(br.x, cy + sc(7.0f)),
                              ImVec2(br.x - sc(8.0f), cy), HUD_BRIGHT);
        panelRect(dl, ImVec2(br.x + sc(4.0f), cy - halfVal - sc(2.0f)),
                  ImVec2(br.x + sc(12.0f) + sz.x, cy + halfVal + sc(2.0f)), sc(3.0f));
        dl->AddText(ImVec2(br.x + sc(8.0f), cy - halfVal), HUD_BRIGHT, abuf);
    } else {
        dl->AddTriangleFilled(ImVec2(tl.x, cy - sc(7.0f)), ImVec2(tl.x, cy + sc(7.0f)),
                              ImVec2(tl.x + sc(8.0f), cy), HUD_BRIGHT);
        panelRect(dl, ImVec2(tl.x - sc(12.0f) - sz.x, cy - halfVal - sc(2.0f)),
                  ImVec2(tl.x - sc(4.0f), cy + halfVal + sc(2.0f)), sc(3.0f));
        dl->AddText(ImVec2(tl.x - sc(8.0f) - sz.x, cy - halfVal), HUD_BRIGHT, abuf);
    }
}

} /* namespace artouste::ui::hud_widgets */
