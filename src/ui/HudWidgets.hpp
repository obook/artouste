/*
 * HudWidgets.hpp
 * Petits utilitaires de dessin du HUD avec Dear ImGui (facteur d'échelle sc(),
 * couleurs, panneau de fond, lignes, cadran rond, rubans de cap et d'altitude),
 * partagés par les unités de compilation du HUD (Hud.cpp et HudOverlay.cpp).
 * Regroupés ici en éléments "inline" (C++17) pour une seule définition partagée.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include <imgui.h>

#include <cmath>
#include <cstdio>

namespace artouste::ui::hud_widgets {

/* Facteur d'échelle du HUD, calé sur la résolution (1 = référence 1280x720). Mis à jour
 * une fois par image par Hud::updateScale ; toutes les dimensions en pixels du HUD
 * passent par sc() pour grandir proportionnellement avec la fenêtre. À scale = 1, sc(x)
 * renvoie x : le rendu 720p reste identique. */
inline float g_scale = 1.0f;
inline float sc(float px) noexcept { return px * g_scale; }

/* Couleurs du HUD : vert assez opaque, plus un noir de contour qui rend tout
 * lisible sur fond clair (ciel) comme sombre, sans masquer la vue. */
inline const ImU32 HUD_GREEN  = IM_COL32(64, 255, 112, 255);   /* vert instrument, net */
inline const ImU32 HUD_BRIGHT = IM_COL32(150, 255, 175, 255);  /* bande nominale, plus vif */
inline const ImU32 HUD_PANEL  = IM_COL32(15, 15, 15, 95);      /* fond translucide, comme les panneaux des coins */
inline const ImU32 HUD_AMBER  = IM_COL32(255, 170, 40, 255);   /* jaune-orangé : surveiller */
inline const ImU32 HUD_RED    = IM_COL32(255, 70, 70, 255);    /* rouge : limite franchie */
inline const ImU32 HUD_CYAN   = IM_COL32(80, 220, 255, 255);   /* cyan : point d'hélipad, distinct des lieux */
inline const ImU32 HUD_HAPI_GREEN = IM_COL32(20, 255, 60, 255); /* vert HAPI (plus vif que HUD_GREEN), point de pad sur la pente */

/* État de la LED d'alarme d'un cadran : None = pas de LED sur ce cadran, Off = LED
 * présente mais éteinte (paramètre pas encore significatif, ex. rotor avant sa
 * première mise en régime), puis vert (normal), jaune (surveiller), rouge (limite
 * franchie). */
enum class GaugeLed { None, Off, Green, Yellow, Red };

/* Ouvre un petit panneau sans décor, ancré à un coin de l'écran.
 * Le pivot (0 ou 1 sur chaque axe) indique de quel coin il s'agit. */
inline void corner(const char* id, const ImVec2& pos, const ImVec2& pivot) {
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowBgAlpha(0.30f);
    ImGui::Begin(id, nullptr, flags);
}

inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* Fond sombre translucide derrière un élément, façon panneau des quatre coins.
 * C'est lui qui assure le contraste : le vert est donc dessiné net, sans contour. */
inline void panelRect(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float rounding) {
    dl->AddRectFilled(a, b, HUD_PANEL, rounding);
}

inline void hudLine(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 col, float th) {
    dl->AddLine(a, b, col, th);
}

inline void hudCircle(ImDrawList* dl, const ImVec2& c, float r, ImU32 col, int seg, float th) {
    dl->AddCircle(c, r, col, seg, th);
}

/* Texte centré horizontalement sur x (lisible grâce au fond translucide). */
inline void centeredText(ImDrawList* dl, float x, float y, ImU32 col, const char* text) {
    dl->AddText(ImVec2(x - ImGui::CalcTextSize(text).x * 0.5f, y), col, text);
}

/* Cadran rond vert façon instrument : fond translucide, cercle, graduations,
 * éventuelle bande de régime nominal (vert plus vif), aiguille, libellé et valeur,
 * plus une LED d'alarme facultative en haut à droite dans l'instrument.
 * Le cadran balaie 270 degrés, ouverture en bas (comme un vrai instrument). */
inline void gauge(ImDrawList* dl, float cx, float cy, float r, float value, float vmin,
                  float vmax, float bandMin, float bandMax, const char* label,
                  const char* valueText, GaugeLed led = GaugeLed::None, bool ledBlinkOn = true) {
    const float a0    = 2.3562f;  /* 135 deg : départ en bas à gauche */
    const float sweep = 4.7124f;  /* 270 deg de balayage, ouverture en bas */

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

/* Ruban d'altitude vertical, à gauche de l'image : même principe que la boussole
 * mais en hauteur. L'échelle (en mètres) défile, l'altitude courante reste au centre
 * sous un repère, avec sa valeur chiffrée à droite du ruban. Graduations tous les
 * 10 m, libellé tous les 50 m. */
inline void altitudeTape(ImDrawList* dl, float left, float cy, float width, float halfHeight,
                         float altitude) {
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

    /* Repère central fixe (triangle pointant vers le ruban) et altitude courante. */
    dl->AddTriangleFilled(ImVec2(br.x, cy - sc(7.0f)), ImVec2(br.x, cy + sc(7.0f)),
                          ImVec2(br.x - sc(8.0f), cy), HUD_BRIGHT);
    char abuf[12];
    std::snprintf(abuf, sizeof(abuf), "%.0f", static_cast<double>(altitude));
    const ImVec2 sz = ImGui::CalcTextSize(abuf);
    panelRect(dl, ImVec2(br.x + sc(4.0f), cy - halfVal - sc(2.0f)),
              ImVec2(br.x + sc(12.0f) + sz.x, cy + halfVal + sc(2.0f)), sc(3.0f));
    dl->AddText(ImVec2(br.x + sc(8.0f), cy - halfVal), HUD_BRIGHT, abuf);
}

}  /* namespace artouste::ui::hud_widgets */
