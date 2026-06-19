// Générateurs de géométrie procédurale pour M1 : en l'absence de modèle 3D
// importé, on construit l'hélico et le terrain à partir de primitives simples.
// Chaque fonction renvoie des données CPU (MeshData) à téléverser dans un Mesh.

#pragma once

#include "render/Mesh.hpp"

#include <vector>

namespace artouste::render::primitives {

struct MeshData {
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
};

// Boîte centrée sur l'origine, définie par ses demi-dimensions.
[[nodiscard]] MeshData box(const vec3& halfExtents, const vec3& color);

// Plan horizontal (y = 0) en damier, centré sur l'origine.
// halfSize : demi-côté du plan (m) ; cells : nombre de cases par axe.
[[nodiscard]] MeshData groundGrid(float halfSize, int cells, const vec3& colorA,
                                  const vec3& colorB);

// Jeu de pales réparties autour de l'axe Y, plus un moyeu central.
// Les pales s'étendent dans le plan XZ ; l'animation se fait en pivotant le
// noeud entier autour de Y à l'affichage.
[[nodiscard]] MeshData bladeSet(int count, float length, float chord, float thickness,
                                const vec3& color);

// Disque plat dans le plan XZ (normale +Y), centré sur l'origine. Sert à
// l'ombre portée au sol.
[[nodiscard]] MeshData disc(float radius, int segments, const vec3& color);

}  // namespace artouste::render::primitives
