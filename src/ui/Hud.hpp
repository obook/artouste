// HUD transparent : affiche les informations de vol en overlay
// via Dear ImGui, par-dessus la scène 3D. Activable/désactivable (touche H).

#pragma once

struct GLFWwindow;

namespace artouste::ui {

// Valeurs affichées, déjà converties dans les unités du HUD.
struct HudData {
    float altitudeM      = 0.0f;
    float airspeedKt     = 0.0f;
    float headingDeg     = 0.0f;
    float varioFpm       = 0.0f;
    float collectivePct  = 0.0f;
    float pedals         = 0.0f;  // [-1, +1]
    float rotorPct       = 0.0f;
};

class Hud {
public:
    void init(GLFWwindow* window);
    void shutdown();

    // Construit et dessine l'overlay pour une image : HUD si showHud, bandeau
    // de pause si paused (une seule frame ImGui).
    void render(const HudData& data, bool showHud, bool paused);

    [[nodiscard]] bool ready() const noexcept { return m_ready; }

private:
    bool m_ready = false;
};

}  // namespace artouste::ui
