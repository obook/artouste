/*
 * Hud.hpp
 * HUD transparent : affiche les informations de vol en surimpression,
 * par-dessus la scène 3D, grâce à Dear ImGui. La touche H fait défiler les modes
 * d'affichage (quatre coins, instruments superposés, rien).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include <vector>

struct GLFWwindow;

namespace artouste::ui {

/* Lieu remarquable à signaler : étiquette posée sur la scène 3D (si visible) et
   point sur la minimap. */
struct HudLabel {
    const char* name     = "";
    float       fx       = 0.0f;   /* position écran de l'étiquette (fraction 0-1) */
    float       fy       = 0.0f;
    float       depth    = 1e30f;  /* profondeur caméra (plus petit = plus proche), pour trier */
    bool        onScreen = false;  /* le lieu est devant la caméra et dans le cadre */
    float       mapU     = 0.0f;   /* position sur la minimap : 0 ouest -> 1 est */
    float       mapV     = 0.0f;   /* 0 nord -> 1 sud */
};

/* Mode d'affichage du HUD, parcouru en boucle par la touche H / le bouton B. */
enum class HudMode {
    Corners,  /* quatre panneaux texte dans les coins */
    Overlay,  /* instruments ronds verts superposés (Super HUD) */
    Off       /* aucun affichage */
};

/* Valeurs à afficher, déjà converties dans les unités du HUD. */
struct HudData {
    float       altitudeM     = 0.0f;
    float       airspeedKt    = 0.0f;
    float       headingDeg    = 0.0f;
    float       varioFpm      = 0.0f;
    float       varioMs       = 0.0f;   /* taux de montée en m/s (instrument superposé) */
    float       collectivePct = 0.0f;
    float       rotorPct      = 0.0f;
    float       rotorRpm      = 0.0f;   /* régime rotor en tr/min */
    float       turbineRpm    = 0.0f;   /* régime turbine en tr/min */
    float       exhaustTempC   = 0.0f;  /* température tuyère (T4) en degrés Celsius */
    float       fuelLiters    = 0.0f;   /* carburant restant, en litres */
    const char* turbine       = "";     /* libellé d'état de la turbine */
    bool        assist        = false;  /* mode assisté actif : affiche un repère */
    bool        radio         = false;  /* flux radio en lecture : affiche un voyant "RADIO" */
    int         radioMixPct   = 0;      /* part de la radio dans le crossfade radio/hélico (0 à 100 %) */
    bool        geoValid      = false;  /* coordonnées géographiques disponibles */
    float       lonDeg        = 0.0f;   /* longitude (degrés, + est) */
    float       latDeg        = 0.0f;   /* latitude (degrés, + nord) */
    float       timeOfDaySec  = 0.0f;   /* heure du simulateur (s depuis minuit) */
    float       timeScale     = 1.0f;   /* vitesse du temps (1 = temps réel) */
    bool        colonOn       = true;   /* deux-points de l'horloge HH:MM (clignote 1 Hz) */

    /* Aide à l'atterrissage : hélipad le plus proche en finale (active seulement
       en mode assisté, voir ApplicationHud.cpp). */
    struct PadGuidance {
        bool        active      = false;  /* true seulement si conditions remplies */
        float       dx          = 0.0f;   /* écart latéral en mètres (+ = droite pilote) */
        float       dz          = 0.0f;   /* écart longitudinal en mètres (+ = devant) */
        float       distanceM   = 0.0f;   /* distance 2D au centre du pad */
        float       altAbovePad = 0.0f;   /* altitude au-dessus du pad (pas du sol général) */
        const char* name        = "";     /* nom du pad (pour l'étiquette HUD) */

        /* Score du dernier posé. */
        float       scoreM      = -1.0f;  /* distance au centre au moment du posé, en mètres */
        bool        scored      = false;  /* true pendant SCORE_DISPLAY_S secondes après le posé */
    };
    PadGuidance padGuidance;

    /* Repérage : lieux remarquables et minimap. */
    std::vector<HudLabel> labels;          /* lieux à étiqueter (scène 3D + carte) */
    unsigned int          mapTexId   = 0;  /* orthophoto pour la minimap (0 = pas de carte) */
    float                 mapHeliU   = 0.0f;  /* position de l'appareil sur la carte (0-1) */
    float                 mapHeliV   = 0.0f;
    float                 mapHeadingDeg = 0.0f;  /* cap, pour orienter le marqueur */

    /* Message radio reçu : sous-titre centré en bas, affiché tant que la chaine n'est
       pas vide. Le pointeur appartient à l'appelant (durée de vie gérée côté app). */
    const char*           radioMessage = "";
};

class Hud {
public:
    void init(GLFWwindow* window);
    void shutdown();

    /* Construit et dessine la surimpression d'une image selon le mode choisi,
     * plus le bandeau de pause si paused (le tout en une seule frame ImGui). */
    /* confirmDemo : affiche le panneau de confirmation avant de lancer la démo.
       forceLabels : afficher les étiquettes des lieux même quand le HUD est éteint
       (mode Off), sans les instruments ni la minimap. Utile pour la démo. */
    void render(const HudData& data, HudMode mode, bool paused, bool confirmReset = false,
                bool confirmDemo = false, bool forceLabels = false);

    [[nodiscard]] bool ready() const noexcept { return m_ready; }

private:
    /* Sous-affichages d'une image, appelés par render() selon le mode. 'w'/'h' sont
       les dimensions de l'écran, 'm' la marge depuis les bords. */
    void renderCorners(const HudData& data, float w, float h, float m);
    void renderOverlay(const HudData& data, float w, float h, float m);
    /* Aide à l'atterrissage (réticule + score), dessinée par-dessus tous les modes
       de HUD, y compris quand il est éteint en démo. */
    void renderPadGuidance(const HudData& data, float w, float h);
    void renderLabels(const HudData& data, float w, float h);
    void renderMinimap(const HudData& data, HudMode mode, float m);
    void renderBanners(bool paused, bool confirmReset, bool confirmDemo, float w, float h);
    /* Sous-titre d'un message radio reçu, centré en bas, par-dessus tous les modes. */
    void renderRadioSubtitle(const HudData& data, float w);

    bool m_ready = false;
};

}  /* namespace artouste::ui */
