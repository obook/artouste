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
    float       pedals        = 0.0f;   /* [-1, +1] */
    float       rotorPct      = 0.0f;
    float       rotorRpm      = 0.0f;   /* régime rotor en tr/min */
    float       turbineRpm    = 0.0f;   /* régime turbine en tr/min */
    float       exhaustTempC   = 0.0f;  /* température tuyère (T4) en degrés Celsius */
    float       fuelLiters    = 0.0f;   /* carburant restant, en litres */
    const char* turbine       = "";     /* libellé d'état de la turbine */
    bool        assist        = false;  /* mode assisté actif : affiche un repère */
    bool        geoValid      = false;  /* coordonnées géographiques disponibles */
    float       lonDeg        = 0.0f;   /* longitude (degrés, + est) */
    float       latDeg        = 0.0f;   /* latitude (degrés, + nord) */

    /* Repérage : lieux remarquables et minimap. */
    std::vector<HudLabel> labels;          /* lieux à étiqueter (scène 3D + carte) */
    unsigned int          mapTexId   = 0;  /* orthophoto pour la minimap (0 = pas de carte) */
    float                 mapHeliU   = 0.0f;  /* position de l'appareil sur la carte (0-1) */
    float                 mapHeliV   = 0.0f;
    float                 mapHeadingDeg = 0.0f;  /* cap, pour orienter le marqueur */
};

class Hud {
public:
    void init(GLFWwindow* window);
    void shutdown();

    /* Construit et dessine la surimpression d'une image selon le mode choisi,
     * plus le bandeau de pause si paused (le tout en une seule frame ImGui). */
    void render(const HudData& data, HudMode mode, bool paused, bool confirmReset = false);

    [[nodiscard]] bool ready() const noexcept { return m_ready; }

private:
    bool m_ready = false;
};

}  /* namespace artouste::ui */
