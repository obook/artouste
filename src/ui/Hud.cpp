/*
 * Hud.cpp
 * Affichage tête haute avec Dear ImGui : cycle de vie (init/shutdown), aiguillage
 * d'une image selon le mode (render), panneaux des quatre coins et bandeaux
 * centraux (pause, confirmations). Le mode superposé, les étiquettes des lieux et
 * la minimap sont dans HudOverlay.cpp ; les utilitaires de dessin dans HudWidgets.hpp.
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
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>

namespace artouste::ui {

using namespace hud_widgets;

void Hud::init(GLFWwindow* window) {
    if (m_ready) {
        return;  /* déjà initialisé (ex. menu de démarrage) : un seul contexte ImGui */
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io    = ImGui::GetIO();
    io.IniFilename = nullptr;  /* pas de fichier de réglages sur disque */
    /* ImGui ne doit pas piloter le curseur : sans ce drapeau, son backend GLFW remet
       GLFW_CURSOR_NORMAL à chaque image (pour afficher les curseurs de survol), ce qui
       annulerait le masquage du curseur qu'on impose en plein écran (setFullscreen). */
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGui::StyleColorsDark();
    m_baseStyle = ImGui::GetStyle();  /* style de référence, remis à l'échelle dans updateScale */
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");
    m_ready = true;
}

void Hud::updateScale(int framebufferWidth, int framebufferHeight) {
    if (!m_ready || framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }
    /* Échelle calée sur la taille du framebuffer (référence 1280x720), convertie en
       unités ImGui via DisplayFramebufferScale (sur nos cibles X11/Windows le facteur
       vaut 1 ; sur un écran HiDPI où fenêtre et framebuffer diffèrent, il évite un
       double agrandissement). Le minimum des deux axes garantit que le rang de cadrans
       tient aussi en fenêtre étroite. L'arrondi au quart crée des paliers francs : la
       police n'est pas reconstruite à chaque pixel d'un redimensionnement continu.
       À 1280x720 et à tout plein écran 16:9, rien ne change par rapport à la référence. */
    const ImVec2 fbEchelle = ImGui::GetIO().DisplayFramebufferScale;
    const float  wUnites   = static_cast<float>(framebufferWidth) / fbEchelle.x;
    const float  hUnites   = static_cast<float>(framebufferHeight) / fbEchelle.y;
    const float  brut      = std::min(wUnites / 1280.0f, hUnites / 720.0f);
    const float  scaleFactor = std::clamp(std::round(brut * 4.0f) / 4.0f, 0.75f, 3.5f);
    hud_widgets::g_scale     = scaleFactor;

    /* Reconstruction de la police et remise à l'échelle des espacements uniquement au
       changement de palier, hors NewFrame (l'atlas ne doit pas bouger en pleine frame). */
    if (scaleFactor != m_builtFontScale) {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        ImFontConfig cfg;
        cfg.SizePixels = std::round(13.0f * scaleFactor);  /* police rastérisée à la bonne taille */
        io.Fonts->AddFontDefault(&cfg);
        io.Fonts->Build();
        /* On détruit seulement la texture : le prochain NewFrame du backend la recrée
           depuis l'atlas reconstruit. La créer ici, avant le tout premier NewFrame,
           ferait fuir une texture (CreateDeviceObjects la recréerait par-dessus). */
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        ImGui::GetStyle() = m_baseStyle;
        ImGui::GetStyle().ScaleAllSizes(scaleFactor);
        m_builtFontScale = scaleFactor;
    }
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

void Hud::render(const HudData& data, HudMode mode, bool paused, bool confirmReset,
                 bool confirmDemo, bool forceLabels) {
    if (!m_ready) {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float  w       = display.x;
    const float  h       = display.y;
    const float  m       = sc(14.0f);  /* marge depuis les bords (à l'échelle) */

    if (mode == HudMode::Corners) {
        renderCorners(data, w, h, m);
    } else if (mode == HudMode::Overlay) {
        renderOverlay(data, w, h, m);
    }
    /* mode Off : aucun affichage de vol. */

    /* Aide à l'atterrissage : réticule et score. Elle fait partie du HUD, donc pas de
       HUD (mode Off) => pas d'aide -- sauf en démo, où on la garde comme les étiquettes
       (forceLabels), le HUD y étant éteint par mise en scène et non par choix du pilote.
       La méthode ne dessine rien de toute façon si le guidage n'est pas actif. */
    if (mode != HudMode::Off || forceLabels) {
        renderPadGuidance(data, w, h);
    }

    /* Étiquettes des lieux : affichées avec le HUD, et aussi quand on les force (démo
       HUD éteint). La minimap, elle, ne s'affiche qu'avec le HUD. */
    const bool showLabels = (mode != HudMode::Off) || forceLabels;
    if (showLabels) {
        renderLabels(data, w, h);
    }
    if (mode != HudMode::Off) {
        renderMinimap(data, mode, m);
    }

    /* Sous-titre d'un message radio : par-dessus tous les modes (y compris HUD éteint
       en démo), pour accompagner la transmission entendue. */
    renderRadioSubtitle(data, w);

    /* Alertes de vol (vortex, taux de chute) : par-dessus tous les modes de vol (mais
       pas prioritaires sur les panneaux de confirmation/pause, dessinés ensuite). */
    renderVortexAlert(data, w, h);
    renderSinkRateAlert(data, w, h);

    renderBanners(paused, confirmReset, confirmDemo, w, h);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

/* Ligne de texte du HUD 4 coins colorée selon un état d'alarme : vert instrument
   hérité quand tout est normal (ou alarme inhibée), jaune ou rouge sinon. Mêmes
   états que les LED des cadrans du Super HUD (voir HudAlarms.hpp) : les deux
   affichages signalent la même chose, chacun avec ses moyens. */
static void ligneAlarme(GaugeLed etat, const char* fmt, ...) {
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
        const int  totalSec = static_cast<int>(data.timeOfDaySec) % 86400;
        const int  hh       = totalSec / 3600;
        const int  mm       = (totalSec % 3600) / 60;
        const char sep      = data.colonOn ? ':' : ' ';
        if (data.timeScale == 1.0f) {
            ImGui::Text("HRE  %02d%c%02d", hh, sep, mm);
        } else {
            ImGui::Text("HRE  %02d%c%02d  x%g", hh, sep, mm,
                        static_cast<double>(data.timeScale));
        }
    }
    if (data.geoValid) {
        ImGui::Text("LAT  %.4f %c", static_cast<double>(std::fabs(data.latDeg)),
                    data.latDeg >= 0.0f ? 'N' : 'S');
        ImGui::Text("LON  %.4f %c", static_cast<double>(std::fabs(data.lonDeg)),
                    data.lonDeg >= 0.0f ? 'E' : 'W');
    }
    ImGui::End();

    corner("hud_bl", ImVec2(m, h - m), ImVec2(0.0f, 1.0f));
    ligneAlarme(alarmeTurbine(data), "TURB %s", data.turbine);
    ligneAlarme(alarmeNr(data), "NR   %3.0f %%", static_cast<double>(data.rotorPct));
    ImGui::Text("COLL %3.0f %%", static_cast<double>(data.collectivePct));
    ligneAlarme(alarmeTmp(data), "TMP  %3.0f C", static_cast<double>(data.exhaustTempC));
    const GaugeLed alCarb = alarmeCarb(data);
    if (alCarb == GaugeLed::Red) {
        ligneAlarme(alCarb, "CARB %4.0f L  BAS", static_cast<double>(data.fuelLiters));
    } else {
        ligneAlarme(alCarb, "CARB %4.0f L", static_cast<double>(data.fuelLiters));
    }
    ImGui::End();

    /* Coin bas-droit : voyant du mode assisté et de la radio quand ils sont actifs,
       et le compteur d'images par seconde (dernière ligne, donc pile dans le coin).
       On ne crée le panneau que s'il y a quelque chose à montrer, pour ne pas laisser
       une boîte vide (fps = 0 en capture => rien). */
    if (data.assist || data.radio || data.fps > 0.0f) {
        corner("hud_br", ImVec2(w - m, h - m), ImVec2(1.0f, 1.0f));
        if (data.radio) {
            ImGui::Text("RADIO %d%%", data.radioMixPct);  /* voyant radio (touche K) + balance (-/+) */
        }
        if (data.assist) {
            ImGui::TextUnformatted("MODE ASSISTE");  /* vert hérité, comme les instruments */
        }
        if (data.fps > 0.0f) {
            ImGui::Text("FPS  %3.0f", static_cast<double>(data.fps));  /* cadence lissée */
        }
        ImGui::End();
    }

    ImGui::PopStyleColor(1);
}

void Hud::renderBanners(bool paused, bool confirmReset, bool confirmDemo, float w, float h) {
    constexpr ImGuiWindowFlags bannerFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

    /* Les panneaux de confirmation (reset, démo) sont prioritaires sur le bandeau de pause. */
    if (confirmReset) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("confirm_reset", nullptr, bannerFlags);
        ImGui::Text("       RÉINITIALISER ?");
        ImGui::Text("Replacer l'appareil au départ");
        ImGui::Text("A / O : Oui        B / N : Non");
        ImGui::End();
    } else if (confirmDemo) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("confirm_demo", nullptr, bannerFlags);
        ImGui::Text("     LANCER LA DÉMO ?");
        ImGui::Text("Vol automatique (Dune du Pilat)");
        ImGui::Text("A / O : Oui        B / N : Non");
        ImGui::End();
    } else if (paused) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGui::Begin("pause", nullptr, bannerFlags);
        ImGui::Text("        PAUSE");
        ImGui::Text("P : reprendre    Échap : quitter");
        ImGui::End();
    }
}

void Hud::renderRadioSubtitle(const HudData& data, float w) {
    if (data.radioMessage == nullptr || data.radioMessage[0] == '\0') {
        return;
    }
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

    /* En haut, centré, sous le ruban de cap du HUD complet : le ruban s'étend de
       y = 30 à y = 72 (bord haut 30 + hauteur 40 + fond) et il est dessiné dans le
       foreground draw list, donc PAR-DESSUS cette fenêtre ; il faut rester en dessous.
       Le bas de l'image est occupé par le rang d'instruments, à ne pas recouvrir. */
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, sc(80.0f)), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("radio_msg", nullptr, flags);
    /* Ambre "radio", distinct du vert instrument. */
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.30f, 1.0f));
    ImGui::Text(">> %s", data.radioMessage);
    ImGui::PopStyleColor();
    ImGui::End();
}

void Hud::renderVortexAlert(const HudData& data, float w, float h) {
    /* Seuil d'affichage : on n'alerte qu'une fois le phénomène franchement installé,
       pour éviter un clignotement au moindre effleurement. */
    constexpr float VORTEX_ALERT_MIN = 0.15f;
    if (data.vrsIntensity < VORTEX_ALERT_MIN) {
        return;
    }
    /* Clignotement : bandeau affiché seulement pendant la phase allumée du battement
       des alarmes (~2 Hz), pour attirer l'oeil comme les LED d'alarme. En capture,
       alarmBlinkOn est figé à true (bandeau visible). */
    if (!data.alarmBlinkOn) {
        return;
    }
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

    /* Tiers supérieur, centré : au-dessus de l'appareil et du réticule d'aide au posé,
       sous le ruban de cap du HUD complet, et distinct du sous-titre radio (en haut). */
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.30f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("vortex_alert", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.20f, 1.0f));  /* rouge alarme */
    ImGui::Text("         VORTEX");
    ImGui::Text("REPRENDRE DE LA VITESSE");
    ImGui::PopStyleColor();
    ImGui::End();
}

void Hud::renderSinkRateAlert(const HudData& data, float w, float h) {
    if (!data.sinkRateAlert) {
        return;
    }
    /* Clignotement, comme le vortex et les LED d'alarme (~2 Hz). */
    if (!data.alarmBlinkOn) {
        return;
    }
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

    /* Juste sous l'emplacement du bandeau vortex : les deux peuvent coexister (descente
       rapide a faible vitesse pres du sol) sans se recouvrir. */
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.40f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("sinkrate_alert", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.20f, 1.0f));  /* rouge alarme */
    ImGui::Text("TAUX DE CHUTE");
    ImGui::PopStyleColor();
    ImGui::End();
}

}  /* namespace artouste::ui */
