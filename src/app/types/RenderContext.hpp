/*
 * RenderContext.hpp
 * Grandeurs communes à toutes les étapes du rendu d'une image, calculées une
 * fois et passées aux sous-méthodes. Voir Application::renderScene.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::app {

struct RenderContext {
    vec3 lightDir;
    mat4 proj;
    mat4 view;
    mat4 toRel;     /* translation -m_renderOrigin, appliquée aux modèles */
    vec3 camPosRel; /* position caméra, relative à m_renderOrigin */
    vec3 fogColor;  /* couleur de brume, assombrie la nuit */
    /* 1 en plein jour, 0 le soleil couché. Sert au fond d'écran, à la brume
       ci-dessus et aux fenêtres allumées (voir building.frag). */
    float isDay;
    /* Part des fenêtres allumées à l'heure courante (voir CycleJourNuit.hpp) :
       la ville s'endort au fil de la nuit et se rallume avant le lever. */
    float partFenetresAllumees;
};

/* Allumage de l'éclairage public et des feux de pad : de 0 à 1 entre soleil à
   +3 et -6 degrés (crépuscule civil). isDay, lui, décroît dès le milieu de
   l'après-midi. */
inline float nuitLampadaires(const vec3& lightDir) {
    return 1.0f - glm::smoothstep(-0.10f, 0.05f, lightDir.y);
}

} /* namespace artouste::app */
