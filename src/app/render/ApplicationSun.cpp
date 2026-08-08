/*
 * ApplicationSun.cpp
 * Heure du simulateur et direction du soleil, utilisées par le rendu de la
 * scène (ApplicationRender.cpp), la boucle principale (caméra d'orbite
 * solaire, ApplicationLoop.cpp) et le HUD. Extrait de ApplicationRender.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "app/CycleJourNuit.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app {

float Application::timeOfDaySeconds(float t) const {
    /* On part de l'heure de départ fixée au chargement de la carte
       (m_sunBaseSeconds), puis on avance à m_sunTimeScale fois le temps écoulé
       DEPUIS ce chargement (m_sunOriginSeconds), et non depuis le lancement du
       programme : chaque carte commence donc à son heure de départ. La
       nuit, elle, défile plus vite : m_nightSpeedFactor multiplie l'échelle entre
       le coucher et le lever (clé lune_vitesse, 2 par défaut). Le calcul lui-même
       est dans CycleJourNuit.cpp : c'est une fonction pure, que les tests
       vérifient sans avoir à ouvrir de fenêtre. */
    return heureDuJour(m_sunBaseSeconds, t - m_sunOriginSeconds, m_sunTimeScale,
                       m_nightSpeedFactor);
}

vec3 Application::sunDirection(float t) const {
    /* Course du soleil : midi -> zénith (y max), 6 h / 18 h -> horizon, minuit ->
       sous l'horizon. Le -pi/2 cale midi (43200 s) sur le zénith. Le repère monde a
       X vers l'est et Z vers le sud (voir ApplicationHud) : le grand axe est-ouest
       est donc porté par X (lever à l'est, coucher à l'ouest), et le décalage fixe en
       Z (0.35) incline l'arc vers le sud, comme dans l'hémisphère nord. */
    constexpr float DAY = 86400.0f;
    const float angle = TWO_PI * (timeOfDaySeconds(t) / DAY) - HALF_PI;
    return glm::normalize(vec3{std::cos(angle), std::sin(angle), 0.35f});
}

} /* namespace artouste::app */
