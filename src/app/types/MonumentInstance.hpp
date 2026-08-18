/*
 * MonumentInstance.hpp
 * Un monument 3D de la carte courante : son modèle et la matrice qui le pose.
 *
 * La matrice est calculée une fois au chargement, en coordonnées monde
 * ABSOLUES ; le dessin la compose avec le recalage d'origine de l'image (voir
 * drawMonuments).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <memory>
#include <string>

namespace artouste::render {
class Model;
}

namespace artouste::app {

struct MonumentInstance {
    std::unique_ptr<render::Model> model;
    mat4                           transform{1.0f};
    std::string                    name;
};

} /* namespace artouste::app */
