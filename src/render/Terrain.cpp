/*
 * Terrain.cpp
 * Construit le maillage du sol en damier à partir des primitives, avec
 * deux teintes de vert qui alternent pour figurer l'herbe.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"

#include "render/Primitives.hpp"

namespace artouste::render {

Terrain::Terrain(float halfSize, int cells) {
    const primitives::MeshData data =
        primitives::groundGrid(halfSize, cells, vec3{0.33f, 0.50f, 0.24f},
                               vec3{0.29f, 0.45f, 0.21f});
    m_mesh = Mesh(data.vertices, data.indices);
}

}  /* namespace artouste::render */
