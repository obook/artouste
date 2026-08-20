/*
 * HudCadran.hpp
 * Cadran rond façon instrument, avec sa LED d'alarme facultative, et bille
 * (inclinomètre) au même gabarit de panneau.
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

/* Cadran rond vert façon instrument : fond translucide, cercle, graduations,
 * éventuelle bande de régime nominal (vert plus vif), aiguille, libellé et valeur,
 * plus une LED d'alarme facultative en haut à droite dans l'instrument.
 * Le cadran balaie 270 degrés ; midAngleDeg place la valeur médiane (défaut 270 =
 * tout en haut, ouverture en bas, comme la plupart des instruments). Le vario
 * utilise 180 (médiane à l'horizontale à gauche, ouverture à droite) pour
 * retrouver la lecture du vrai VSI : zéro à 9 heures, montée vers midi. */
inline void gauge(ImDrawList* dl, float cx, float cy, float r, float value, float vmin,
                  float vmax, float bandMin, float bandMax, const char* label,
                  const char* valueText, GaugeLed led = GaugeLed::None, bool ledBlinkOn = true,
                  float midAngleDeg = 270.0f, bool zeroLine = false) {
    const float sweep = 4.7124f;                                /* 270 deg de balayage */
    const float a0    = (midAngleDeg - 135.0f) * 0.017453293f;  /* deg -> rad, médiane centrée */

    panelRect(dl, ImVec2(cx - r - sc(8.0f), cy - r - sc(18.0f)),
              ImVec2(cx + r + sc(8.0f), cy + r + sc(20.0f)), sc(6.0f));
    hudCircle(dl, ImVec2(cx, cy), r, HUD_GREEN, 48, sc(1.5f));

    if (led != GaugeLed::None) {  /* LED d'alarme, coin haut-droit de l'instrument */
        /* Teinte éteinte, réutilisée pour l'état Off ET pour la demi-période sombre du
           clignotement d'une alarme jaune/rouge (ledBlinkOn == false). */
        const ImU32 dim  = IM_COL32(70, 80, 75, 200);
        const bool  warn = (led == GaugeLed::Yellow || led == GaugeLed::Red);
        ImU32       coul = dim;  /* Off, ou alarme dans sa phase éteinte */
        if (led == GaugeLed::Green) {
            coul = HUD_GREEN;  /* le vert (normal) reste allumé en continu */
        } else if (warn && ledBlinkOn) {
            coul = (led == GaugeLed::Yellow) ? HUD_AMBER : HUD_RED;  /* phase allumée */
        }
        const ImVec2 pos(cx + r + sc(2.0f), cy - r - sc(10.0f));
        dl->AddCircleFilled(pos, sc(4.0f), coul);
        dl->AddCircle(pos, sc(4.0f), IM_COL32(0, 0, 0, 160));
    }

    for (int i = 0; i <= 10; ++i) {  /* graduations */
        const float a = a0 + sweep * (static_cast<float>(i) / 10.0f);
        const float c = std::cos(a);
        const float s = std::sin(a);
        hudLine(dl, ImVec2(cx + c * r * 0.82f, cy + s * r * 0.82f),
                 ImVec2(cx + c * r, cy + s * r), HUD_GREEN, sc(1.5f));
    }

    if (zeroLine) {  /* repère de la médiane (vario) : un rayon du centre jusqu'au bord,
                         en pointillés fins, plutôt qu'un trait plein traversant tout
                         le cadran (l'autre moitié retomberait dans l'ouverture, hors
                         échelle) ; se distingue de l'aiguille, qui vient s'y superposer
                         exactement à la valeur zéro. */
        const float midA = a0 + sweep * 0.5f;
        const float c    = std::cos(midA);
        const float s    = std::sin(midA);
        constexpr int DASHES = 16;
        for (int i = 0; i < DASHES; i += 2) {
            const float t0 = static_cast<float>(i) / DASHES;
            const float t1 = static_cast<float>(i + 1) / DASHES;
            hudLine(dl, ImVec2(cx + c * r * 0.9f * t0, cy + s * r * 0.9f * t0),
                     ImVec2(cx + c * r * 0.9f * t1, cy + s * r * 0.9f * t1), HUD_GREEN, sc(1.0f));
        }
    }

    if (bandMax > bandMin) {  /* bande de régime nominal */
        const float t0 = clamp01((bandMin - vmin) / (vmax - vmin));
        const float t1 = clamp01((bandMax - vmin) / (vmax - vmin));
        dl->PathArcTo(ImVec2(cx, cy), r * 0.92f, a0 + sweep * t0, a0 + sweep * t1, 24);
        dl->PathStroke(HUD_BRIGHT, 0, sc(3.0f));
    }

    const float t = clamp01((value - vmin) / (vmax - vmin));  /* aiguille */
    const float a = a0 + sweep * t;
    hudLine(dl, ImVec2(cx, cy),
            ImVec2(cx + std::cos(a) * r * 0.78f, cy + std::sin(a) * r * 0.78f), HUD_GREEN,
            sc(2.0f));
    dl->AddCircleFilled(ImVec2(cx, cy), sc(2.5f), HUD_GREEN);

    centeredText(dl, cx, cy - r - sc(16.0f), HUD_GREEN, label);
    centeredText(dl, cx, cy + r + sc(4.0f), HUD_GREEN, valueText);
}

/* Aiguille-bille : taux de virage en haut (deg/s), bille de dérapage en bas (force
 * spécifique latérale en g). Les deux sont positifs à droite. Aiguille sur un repère
 * et bille au milieu = virage coordonné au taux standard. Le panneau reprend le
 * gabarit du cadran rond pour tenir dans la même rangée. */
inline void aiguilleBille(ImDrawList* dl, float cx, float cy, float r, float virageDegS,
                          float valueG, const char* label) {
    panelRect(dl, ImVec2(cx - r - sc(8.0f), cy - r - sc(18.0f)),
              ImVec2(cx + r + sc(8.0f), cy + r + sc(20.0f)), sc(6.0f));

    /* Aiguille de virage, pivot en bas. Les deux repères marquent le taux standard
       (3 deg/s, un tour en deux minutes) ; la butée est au double. */
    const ImVec2  pivot(cx, cy + r * 0.30f);
    const float   aigLen  = r * 0.95f;
    constexpr float STD_DEG_S  = 3.0f;
    constexpr float STD_INCL   = 0.35f;  /* rad, inclinaison de l'aiguille au taux standard */
    for (float sgn : {-1.0f, 1.0f}) {  /* repères du taux standard */
        const float ar = -1.5707963f + sgn * STD_INCL;
        const float u  = std::cos(ar);
        const float v  = std::sin(ar);
        hudLine(dl, ImVec2(pivot.x + u * aigLen * 0.80f, pivot.y + v * aigLen * 0.80f),
                ImVec2(pivot.x + u * aigLen * 1.05f, pivot.y + v * aigLen * 1.05f), HUD_GREEN,
                sc(1.5f));
    }
    const float incl = std::fmax(-2.0f, std::fmin(2.0f, virageDegS / STD_DEG_S)) * STD_INCL;
    const float ar   = -1.5707963f + incl;
    hudLine(dl, pivot, ImVec2(pivot.x + std::cos(ar) * aigLen, pivot.y + std::sin(ar) * aigLen),
            HUD_BRIGHT, sc(2.0f));
    dl->AddCircleFilled(pivot, sc(2.5f), HUD_GREEN);

    /* Tube de la bille : arc de grand rayon, centre au-dessus, creux vers le bas. */
    const float   tubeR    = r * 1.8f;
    const float   ballR    = sc(6.0f);
    const ImVec2  c(cx, cy + r * 0.72f - tubeR);
    /* Demi-course de la bille : réglée pour que les parois et la bille en butée
       tiennent dans la largeur du panneau (r + 8), pas au-delà. */
    const float   halfSpan = 0.52f;
    /* Les deux parois s'arrêtent au même écart latéral, un rayon de bille au-delà de
       la butée : à angle égal la paroi intérieure (rayon plus court) s'arrêterait
       trop tôt et la bille en butée déborderait du tube. */
    const float bout = std::sin(halfSpan) * tubeR + ballR + sc(2.0f);
    for (float dr : {-(ballR + sc(2.0f)), ballR + sc(2.0f)}) {
        const float demi = std::asin(std::fmin(bout / (tubeR + dr), 1.0f));
        dl->PathArcTo(c, tubeR + dr, 1.5707963f - demi, 1.5707963f + demi, 32);
        dl->PathStroke(HUD_GREEN, 0, sc(1.5f));
    }

    /* Repères de coordination, de part et d'autre du fond du tube. */
    for (float sgn : {-1.0f, 1.0f}) {
        const float a = 1.5707963f + sgn * (ballR + sc(1.0f)) / tubeR;
        const float u = std::cos(a);
        const float v = std::sin(a);
        hudLine(dl, ImVec2(c.x + u * (tubeR - ballR - sc(2.0f)), c.y + v * (tubeR - ballR - sc(2.0f))),
                ImVec2(c.x + u * (tubeR + ballR + sc(2.0f)), c.y + v * (tubeR + ballR + sc(2.0f))),
                HUD_GREEN, sc(1.5f));
    }

    /* Butée à 0,3 g, comme l'instrument réel. L'angle décroît vers la droite (l'axe Y
       de l'écran pointe vers le bas), d'où le signe moins. */
    const float t = std::fmax(-1.0f, std::fmin(1.0f, valueG / 0.3f));
    const float a = 1.5707963f - t * halfSpan;
    dl->AddCircleFilled(ImVec2(c.x + std::cos(a) * tubeR, c.y + std::sin(a) * tubeR), ballR,
                        HUD_BRIGHT);

    centeredText(dl, cx, cy - r - sc(16.0f), HUD_GREEN, label);
}

} /* namespace artouste::ui::hud_widgets */
