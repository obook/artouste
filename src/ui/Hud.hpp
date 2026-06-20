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

struct GLFWwindow;

namespace artouste::ui {

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
    float       fuelLiters    = 0.0f;   /* carburant restant, en litres */
    const char* turbine       = "";     /* libellé d'état de la turbine */
    bool        assist        = false;  /* mode assisté actif : affiche un repère */
};

class Hud {
public:
    void init(GLFWwindow* window);
    void shutdown();

    /* Construit et dessine la surimpression d'une image selon le mode choisi,
     * plus le bandeau de pause si paused (le tout en une seule frame ImGui). */
    void render(const HudData& data, HudMode mode, bool paused);

    [[nodiscard]] bool ready() const noexcept { return m_ready; }

private:
    bool m_ready = false;
};

}  /* namespace artouste::ui */
