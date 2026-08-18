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
};

} /* namespace artouste::app */
