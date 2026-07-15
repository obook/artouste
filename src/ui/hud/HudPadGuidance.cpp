/*
 * HudPadGuidance.cpp
 * Aide à l'atterrissage : réticule de centrage hélipad et score au posé.
 * Extrait de HudOverlay.cpp.
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
 * reticleCentrage
 * Réticule de centrage hélipad : cercle de référence, croix centrale fixe (cible)
 * et deux barres de déviation (latérale et longitudinale) qui s'écartent quand on
 * n'est pas aligné. Vert quand on est centré, jaune sinon. Aide à l'atterrissage
 * du mode assisté, visible en finale basse vitesse.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 */
static void reticleCentrage(ImDrawList* dl, float cx, float cy,
                            float dx, float dz, float distanceM) {
    const float RAYON           = sc(55.0f);   /* rayon du cercle de référence */
    const float ECHELLE_M       = sc(5.0f);    /* 1 m = ECHELLE_M pixels dans le réticule */
    const float BARRE_DEMI      = sc(35.0f);   /* demi-longueur des barres de déviation */
    const float BARRE_EPAISSEUR = sc(2.5f);

    /* Barres bornées au rayon (au-delà, on sait juste "loin dans cette direction"). */
    const float barX = std::clamp(dx * ECHELLE_M, -RAYON, RAYON);
    const float barZ = std::clamp(dz * ECHELLE_M, -RAYON, RAYON);

    /* Fond translucide et cercle de référence. */
    panelRect(dl, ImVec2(cx - RAYON - sc(10.0f), cy - RAYON - sc(10.0f)),
              ImVec2(cx + RAYON + sc(10.0f), cy + RAYON + sc(10.0f)), sc(8.0f));
    hudCircle(dl, ImVec2(cx, cy), RAYON, HUD_GREEN, 48, sc(1.5f));

    /* Croix centrale fixe : la cible (centre du pad). */
    hudLine(dl, ImVec2(cx - sc(12.0f), cy), ImVec2(cx + sc(12.0f), cy), HUD_GREEN, sc(1.5f));
    hudLine(dl, ImVec2(cx, cy - sc(12.0f)), ImVec2(cx, cy + sc(12.0f)), HUD_GREEN, sc(1.5f));

    /* Vert si centré (moins d'un mètre sur les deux axes), jaune sinon. */
    const ImU32 coulBarre = (std::fabs(dx) < 1.0f && std::fabs(dz) < 1.0f)
                          ? IM_COL32(64, 255, 112, 255)
                          : IM_COL32(255, 200, 64, 255);

    /* Barre latérale (verticale) : se décale vers le pad sur l'axe gauche/droite. */
    hudLine(dl, ImVec2(cx + barX, cy - BARRE_DEMI), ImVec2(cx + barX, cy + BARRE_DEMI),
            coulBarre, BARRE_EPAISSEUR);
    /* Barre longitudinale (horizontale) : monte si le pad est devant. */
    hudLine(dl, ImVec2(cx - BARRE_DEMI, cy - barZ), ImVec2(cx + BARRE_DEMI, cy - barZ),
            coulBarre, BARRE_EPAISSEUR);

    /* Distance au centre du pad, sous le réticule. */
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%.0f m", static_cast<double>(distanceM));
    centeredText(dl, cx, cy + RAYON + sc(6.0f), HUD_GREEN, buf);
}

void Hud::renderPadGuidance(const HudData& data, float w, float h) {
    /* Aide à l'atterrissage : réticule de centrage et score au posé. Dessinée sur la
       liste d'avant-plan, donc visible dans tous les modes de HUD, y compris en démo
       (HUD éteint). Active seulement quand le calcul l'a remplie (mode assisté ou démo). */
    if (!data.padGuidance.active && !data.padGuidance.scored) {
        return;
    }
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const float cx = w * 0.5f;
    const float cy = h * 0.72f;

    /* Réticule de centrage, en finale basse vitesse. */
    if (data.padGuidance.active) {
        reticleCentrage(dl, cx, cy, data.padGuidance.dx, data.padGuidance.dz,
                        data.padGuidance.distanceM);
    }

    /* Score du posé pendant quelques secondes : mention et couleur selon la distance. */
    if (data.padGuidance.scored) {
        const float d = data.padGuidance.scoreM;
        const char* mention;
        ImU32       couleur;
        if (d < 1.0f) {
            mention = "PARFAIT";
            couleur = IM_COL32(64, 255, 112, 255);   /* vert */
        } else if (d < 3.0f) {
            mention = "BON";
            couleur = IM_COL32(64, 255, 112, 255);   /* vert */
        } else if (d < 5.0f) {
            mention = "ACCEPTABLE";
            couleur = IM_COL32(255, 200, 64, 255);   /* jaune */
        } else {
            mention = "HORS PAD";
            couleur = IM_COL32(220, 60, 60, 255);    /* rouge */
        }
        char buf[48];
        std::snprintf(buf, sizeof(buf), "Pose : %.1f m  %s", static_cast<double>(d), mention);
        /* Au-dessus du réticule s'il est visible (sous, on tomberait sur le rang de
           cadrans du HUD complet), sinon centré sur la position du réticule. */
        const ImVec2 ts = ImGui::CalcTextSize(buf);
        const float  ty = data.padGuidance.active ? (cy - sc(92.0f)) : cy;
        const float  tx = cx - ts.x * 0.5f;
        panelRect(dl, ImVec2(tx - sc(6.0f), ty - sc(4.0f)),
                  ImVec2(tx + ts.x + sc(6.0f), ty + ts.y + sc(4.0f)), sc(4.0f));
        dl->AddText(ImVec2(tx, ty), couleur, buf);
    }
}

}  /* namespace artouste::ui */
