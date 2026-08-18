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

/* Drapeaux communs à toutes les fenêtres du HUD : ni décor, ni interaction, ni
 * position mémorisée d'une session à l'autre. Le HUD ne se manipule pas, il
 * s'affiche. */
inline constexpr ImGuiWindowFlags HUD_FLAGS =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

/* Ouvre un petit panneau sans décor, ancré à un coin de l'écran.
 * Le pivot (0 ou 1 sur chaque axe) indique de quel coin il s'agit. */
inline void corner(const char* id, const ImVec2& pos, const ImVec2& pivot) {
    constexpr ImGuiWindowFlags flags = HUD_FLAGS | ImGuiWindowFlags_NoFocusOnAppearing;
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

} /* namespace artouste::ui::hud_widgets */
