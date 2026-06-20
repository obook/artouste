/*
 * Hud.hpp
 * HUD transparent : affiche les informations de vol en surimpression,
 * par-dessus la scène 3D, grâce à Dear ImGui. La touche H l'affiche ou le masque.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

struct GLFWwindow;

namespace artouste::ui {

/* Valeurs à afficher, déjà converties dans les unités du HUD. */
struct HudData {
    float altitudeM      = 0.0f;
    float airspeedKt     = 0.0f;
    float headingDeg     = 0.0f;
    float varioFpm       = 0.0f;
    float collectivePct  = 0.0f;
    float pedals         = 0.0f;  /* [-1, +1] */
    float rotorPct       = 0.0f;
};

class Hud {
public:
    void init(GLFWwindow* window);
    void shutdown();

    /* Construit et dessine la surimpression d'une image : le HUD si showHud,
     * le bandeau de pause si paused (le tout en une seule frame ImGui). */
    void render(const HudData& data, bool showHud, bool paused);

    [[nodiscard]] bool ready() const noexcept { return m_ready; }

private:
    bool m_ready = false;
};

}  /* namespace artouste::ui */
