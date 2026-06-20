/*
 * Clock.hpp
 * Petite horloge de boucle : à chaque image, elle mesure le temps écoulé
 * depuis l'image précédente (dt) ainsi que le temps total depuis le démarrage.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include <GLFW/glfw3.h>

namespace artouste::app {

class Clock {
public:
    /* À appeler une seule fois par image, au début de la boucle. */
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

}  /* namespace artouste::app */
