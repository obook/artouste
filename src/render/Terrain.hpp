// Terrain plat en damier : un simple plan herbeux donnant des
// repères visuels pour percevoir le mouvement. Pas de relief pour l'instant.

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

}  // namespace artouste::render
