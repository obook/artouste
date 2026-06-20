/*
 * FlightAssist.hpp
 * Mode assisté : couche de confort posée par-dessus les commandes du pilote,
 * sans toucher à la physique. Elle aide à tenir l'Alouette II (appareil exigeant,
 * sans servo-commandes) en compensant le lacet, en ramenant le cyclique au neutre,
 * en lissant les inputs et en bornant la vitesse du collectif. La bascule entre
 * physique brute et mode assisté est progressive, jamais instantanée.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/Controls.hpp"

namespace artouste::physics {

class FlightAssist {
public:
    /* Bascule l'assistance. La transition reste progressive : le retour de
     * active() change tout de suite, mais l'intensité monte ou descend en douceur. */
    void toggle() noexcept { m_active = !m_active; }

    /* Corrige les commandes brutes du pilote selon l'intensité courante de
     * l'assistance, et renvoie les commandes à transmettre à la physique. */
    Controls apply(const Controls& raw, float dt) noexcept;

    [[nodiscard]] bool  active() const noexcept { return m_active; }
    [[nodiscard]] float intensity() const noexcept { return m_intensity; }

private:
    /* Recale l'état interne sur les commandes brutes, pour qu'une (ré)activation
     * de l'assistance reparte sans à-coup depuis la position réelle des commandes. */
    void syncTo(const Controls& raw) noexcept;

    bool  m_active    = false;  /* assistance demandée (true) ou physique brute (false) */
    float m_intensity = 0.0f;   /* 0 = aucune correction, 1 = correction pleine (transition) */

    /* Valeurs lissées conservées d'une image à l'autre (sortie de l'assistance). */
    float m_cyclicLateral      = 0.0f;
    float m_cyclicLongitudinal = 0.0f;
    float m_pedals             = 0.0f;
    float m_collective         = 0.0f;
};

}  /* namespace artouste::physics */
