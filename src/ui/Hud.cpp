/*
 * Hud.cpp
 * Construction de l'affichage tête haute avec Dear ImGui : quatre panneaux
 * dans les coins de l'écran, plus un bandeau de pause centré.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "ui/Hud.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <cmath>
#include <cstdio>

namespace artouste::ui {

namespace {

/* Ouvre un petit panneau sans décor, ancré à un coin de l'écran.
 * Le pivot (0 ou 1 sur chaque axe) indique de quel coin il s'agit. */
void corner(const char* id, const ImVec2& pos, const ImVec2& pivot) {
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowBgAlpha(0.30f);
    ImGui::Begin(id, nullptr, flags);
}

float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* Couleurs du HUD : vert assez opaque, plus un noir de contour qui rend tout
 * lisible sur fond clair (ciel) comme sombre, sans masquer la vue. */
const ImU32 HUD_GREEN  = IM_COL32(64, 255, 112, 255);   /* vert instrument, net */
const ImU32 HUD_BRIGHT = IM_COL32(150, 255, 175, 255);  /* bande nominale, plus vif */
const ImU32 HUD_PANEL  = IM_COL32(15, 15, 15, 95);      /* fond translucide, comme les panneaux des coins */

/* Fond sombre translucide derrière un élément, façon panneau des quatre coins.
 * C'est lui qui assure le contraste : le vert est donc dessiné net, sans contour. */
void panelRect(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float rounding) {
    dl->AddRectFilled(a, b, HUD_PANEL, rounding);
}

void hudLine(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 col, float th) {
    dl->AddLine(a, b, col, th);
}

void hudCircle(ImDrawList* dl, const ImVec2& c, float r, ImU32 col, int seg, float th) {
    dl->AddCircle(c, r, col, seg, th);
}

/* Texte centré horizontalement sur x (lisible grâce au fond translucide). */
void centeredText(ImDrawList* dl, float x, float y, ImU32 col, const char* text) {
    dl->AddText(ImVec2(x - ImGui::CalcTextSize(text).x * 0.5f, y), col, text);
}

/* Cadran rond vert façon instrument : fond translucide, cercle, graduations,
 * éventuelle bande de régime nominal (vert plus vif), aiguille, libellé et valeur.
 * Le cadran balaie 270 degrés, ouverture en bas (comme un vrai instrument). */
void gauge(ImDrawList* dl, float cx, float cy, float r, float value, float vmin, float vmax,
           float bandMin, float bandMax, const char* label, const char* valueText) {
    const float a0    = 2.3562f;  /* 135 deg : départ en bas à gauche */
    const float sweep = 4.7124f;  /* 270 deg de balayage, ouverture en bas */

    panelRect(dl, ImVec2(cx - r - 8.0f, cy - r - 18.0f), ImVec2(cx + r + 8.0f, cy + r + 20.0f), 6.0f);
    hudCircle(dl, ImVec2(cx, cy), r, HUD_GREEN, 48, 1.5f);

    for (int i = 0; i <= 10; ++i) {  /* graduations */
        const float a = a0 + sweep * (static_cast<float>(i) / 10.0f);
        const float c = std::cos(a);
        const float s = std::sin(a);
        hudLine(dl, ImVec2(cx + c * r * 0.82f, cy + s * r * 0.82f),
                 ImVec2(cx + c * r, cy + s * r), HUD_GREEN, 1.5f);
    }

    if (bandMax > bandMin) {  /* bande de régime nominal */
        const float t0 = clamp01((bandMin - vmin) / (vmax - vmin));
        const float t1 = clamp01((bandMax - vmin) / (vmax - vmin));
        dl->PathArcTo(ImVec2(cx, cy), r * 0.92f, a0 + sweep * t0, a0 + sweep * t1, 24);
        dl->PathStroke(HUD_BRIGHT, 0, 3.0f);
    }

    const float t = clamp01((value - vmin) / (vmax - vmin));  /* aiguille */
    const float a = a0 + sweep * t;
    hudLine(dl, ImVec2(cx, cy),
            ImVec2(cx + std::cos(a) * r * 0.78f, cy + std::sin(a) * r * 0.78f), HUD_GREEN, 2.0f);
    dl->AddCircleFilled(ImVec2(cx, cy), 2.5f, HUD_GREEN);

    centeredText(dl, cx, cy - r - 16.0f, HUD_GREEN, label);
    centeredText(dl, cx, cy + r + 4.0f, HUD_GREEN, valueText);
}

/* Ramène un angle dans [-180, +180] degrés (plus court chemin). */
float wrap180(float a) {
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
void headingTape(ImDrawList* dl, float cx, float top, float halfWidth, float height,
                 float heading) {
    const float pxPerDeg = 4.0f;
    const float spanDeg  = halfWidth / pxPerDeg;  /* demi-plage visible, en degrés */
    const ImVec2 tl{cx - halfWidth, top};
    const ImVec2 br{cx + halfWidth, top + height};

    panelRect(dl, ImVec2(tl.x, top - 24.0f), ImVec2(br.x, br.y + 2.0f), 4.0f);
    dl->AddRect(tl, br, HUD_GREEN, 0.0f, 0, 1.5f);
    dl->PushClipRect(tl, br, true);

    const int start = static_cast<int>(std::floor((heading - spanDeg) / 5.0f)) * 5;
    const int end   = static_cast<int>(std::ceil((heading + spanDeg) / 5.0f)) * 5;
    for (int v = start; v <= end; v += 5) {
        const float x = cx + wrap180(static_cast<float>(v) - heading) * pxPerDeg;
        const bool  major = (v % 10) == 0;
        hudLine(dl, ImVec2(x, top), ImVec2(x, top + (major ? height * 0.5f : height * 0.3f)),
                 HUD_GREEN, 1.5f);
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

    /* Repère central fixe (triangle pointant vers le ruban) et cap courant chiffré. */
    dl->AddTriangleFilled(ImVec2(cx - 6.0f, top - 1.0f), ImVec2(cx + 6.0f, top - 1.0f),
                          ImVec2(cx, top + 7.0f), HUD_BRIGHT);
    const int hdg = (static_cast<int>(heading + 0.5f) % 360 + 360) % 360;
    char      hbuf[8];
    std::snprintf(hbuf, sizeof(hbuf), "%03d", hdg);
    centeredText(dl, cx, top - 20.0f, HUD_BRIGHT, hbuf);
}

/* Ruban d'altitude vertical, à gauche de l'image : même principe que la boussole
 * mais en hauteur. L'échelle (en mètres) défile, l'altitude courante reste au centre
 * sous un repère, avec sa valeur chiffrée à droite du ruban. Graduations tous les
 * 10 m, libellé tous les 50 m. */
void altitudeTape(ImDrawList* dl, float left, float cy, float width, float halfHeight,
                  float altitude) {
    const float  pxPerM    = 2.0f;
    const int    minorStep = 10;  /* m entre graduations */
    const int    majorStep = 50;  /* m entre libellés */
    const ImVec2 tl{left, cy - halfHeight};
    const ImVec2 br{left + width, cy + halfHeight};
    const float  half = ImGui::GetTextLineHeight() * 0.5f;

    panelRect(dl, tl, br, 4.0f);
    dl->AddRect(tl, br, HUD_GREEN, 0.0f, 0, 1.5f);
    dl->PushClipRect(tl, br, true);

    const float rangeM = halfHeight / pxPerM;  /* demi-plage visible, en mètres */
    const int   start  = static_cast<int>(std::floor((altitude - rangeM) / minorStep)) * minorStep;
    const int   end    = static_cast<int>(std::ceil((altitude + rangeM) / minorStep)) * minorStep;
    for (int v = start; v <= end; v += minorStep) {
        const float y     = cy - (static_cast<float>(v) - altitude) * pxPerM;
        const bool  major = (v % majorStep) == 0;
        hudLine(dl, ImVec2(br.x - (major ? width * 0.45f : width * 0.25f), y), ImVec2(br.x, y),
                HUD_GREEN, 1.5f);
        if (major && v >= 0) {
            char buf[12];
            std::snprintf(buf, sizeof(buf), "%d", v);
            dl->AddText(ImVec2(tl.x + 4.0f, y - half), HUD_GREEN, buf);
        }
    }
    dl->PopClipRect();

    /* Repère central fixe (triangle pointant vers le ruban) et altitude courante. */
    dl->AddTriangleFilled(ImVec2(br.x, cy - 7.0f), ImVec2(br.x, cy + 7.0f),
                          ImVec2(br.x - 8.0f, cy), HUD_BRIGHT);
    char abuf[12];
    std::snprintf(abuf, sizeof(abuf), "%.0f", static_cast<double>(altitude));
    const ImVec2 sz = ImGui::CalcTextSize(abuf);
    panelRect(dl, ImVec2(br.x + 4.0f, cy - half - 2.0f), ImVec2(br.x + 12.0f + sz.x, cy + half + 2.0f),
              3.0f);
    dl->AddText(ImVec2(br.x + 8.0f, cy - half), HUD_BRIGHT, abuf);
}

}  /* namespace */

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

void Hud::render(const HudData& data, HudMode mode, bool paused) {
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
        corner("hud_tl", ImVec2(m, m), ImVec2(0.0f, 0.0f));
        ImGui::Text("ALT  %5.0f m", static_cast<double>(data.altitudeM));
        ImGui::Text("V/S  %+5.0f ft/min", static_cast<double>(data.varioFpm));
        ImGui::End();

        corner("hud_tr", ImVec2(w - m, m), ImVec2(1.0f, 0.0f));
        ImGui::Text("IAS  %4.0f kt", static_cast<double>(data.airspeedKt));
        ImGui::Text("HDG  %03.0f", static_cast<double>(data.headingDeg));
        ImGui::End();

        corner("hud_bl", ImVec2(m, h - m), ImVec2(0.0f, 1.0f));
        ImGui::Text("TURB %s", data.turbine);
        ImGui::Text("NR   %3.0f %%", static_cast<double>(data.rotorPct));
        ImGui::Text("COLL %3.0f %%", static_cast<double>(data.collectivePct));
        ImGui::End();

        corner("hud_br", ImVec2(w - m, h - m), ImVec2(1.0f, 1.0f));
        ImGui::Text("PALONNIER");
        ImGui::ProgressBar(data.pedals * 0.5f + 0.5f, ImVec2(140.0f, 0.0f), "");
        ImGui::End();
    } else if (mode == HudMode::Overlay) {
        /* Super HUD : rang d'instruments ronds verts superposés en bas de l'image
         * (Priorité 1 de PANEL.md), assez bas pour ne pas gêner la vue de vol. */
        ImDrawList* dl = ImGui::GetForegroundDrawList();

        /* Ruban de cap qui défile, en haut de l'image. */
        headingTape(dl, w * 0.5f, 30.0f, 230.0f, 40.0f, data.headingDeg);

        /* Ruban d'altitude vertical, à gauche de l'image. */
        altitudeTape(dl, 24.0f, h * 0.5f, 60.0f, h * 0.28f, data.altitudeM);

        /* Valeurs pré-formatées (formats littéraux : pas de format dynamique). */
        char nr[16], turb[16], ias[16], vs[16], coll[16];
        std::snprintf(nr,   sizeof(nr),   "%.0f",  static_cast<double>(data.rotorRpm));
        std::snprintf(turb, sizeof(turb), "%.0f",  static_cast<double>(data.turbineRpm));
        std::snprintf(ias,  sizeof(ias),  "%.0f",  static_cast<double>(data.airspeedKt));
        std::snprintf(vs,   sizeof(vs),   "%+.1f", static_cast<double>(data.varioMs));
        std::snprintf(coll, sizeof(coll), "%.0f",  static_cast<double>(data.collectivePct));

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
    }
    /* mode Off : aucun affichage de vol. */

    if (paused) {
        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGui::Begin("pause", nullptr, flags);
        ImGui::Text("        PAUSE");
        ImGui::Text("P : reprendre    Échap : quitter");
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  /* namespace artouste::ui */
