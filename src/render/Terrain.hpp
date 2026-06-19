/*
 * Terrain.hpp
 * Sol plat en damier : un plan herbeux qui sert de repère visuel pour
 * percevoir le déplacement de l'appareil. Pas de relief pour l'instant.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include "render/Mesh.hpp"

namespace artouste::render {

class Terrain {
public:
    Terrain(float halfSize, int cells);

    void draw() const { m_mesh.draw(); }

private:
    Mesh m_mesh;
};

}  /* namespace artouste::render */
