// Horloge de boucle : mesure le pas de temps de rendu (dt) et le temps total.
// Le pas fixe de simulation physique sera ajouté à M3.

#pragma once

#include <GLFW/glfw3.h>

namespace artouste::app {

class Clock {
public:
    // A appeler une fois par image, en début de boucle.
    void tick() noexcept {
        const double now = glfwGetTime();
        m_dt              = now - m_last;
        m_last            = now;
        m_elapsed         = now;
    }

    [[nodiscard]] double dt() const noexcept { return m_dt; }
    [[nodiscard]] double elapsed() const noexcept { return m_elapsed; }

private:
    double m_last    = 0.0;
    double m_dt      = 0.0;
    double m_elapsed = 0.0;
};

}  // namespace artouste::app
