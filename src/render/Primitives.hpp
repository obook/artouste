/*
 * Primitives.hpp
 * Fabrique des formes géométriques de base (boîte, plan, pales, disque)
 * par le calcul, sans modèle 3D importé. C'est ainsi qu'on construit
 * l'hélicoptère et le terrain. Chaque fonction renvoie les sommets et
 * indices (MeshData) prêts à devenir un Mesh.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Mesh.hpp"

#include <vector>

namespace artouste::render::primitives {

struct MeshData {
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
};

/* Boîte centrée sur l'origine, définie par ses demi-dimensions. */
[[nodiscard]] MeshData box(const vec3& halfExtents, const vec3& color);

/*
 * Plan horizontal (y = 0) en damier, centré sur l'origine.
 * halfSize : demi-côté du plan (m) ; cells : nombre de cases par axe.
 */
[[nodiscard]] MeshData groundGrid(float halfSize, int cells, const vec3& colorA,
                                  const vec3& colorB);

/*
 * Jeu de pales réparties autour de l'axe Y, plus un moyeu central.
 * Les pales s'étendent dans le plan XZ ; l'animation se fait en faisant
 * tourner tout le rotor autour de Y au moment de l'affichage.
 */
[[nodiscard]] MeshData bladeSet(int count, float length, float chord, float thickness,
                                const vec3& color);

/*
 * Disque plat dans le plan XZ (normale +Y), centré sur l'origine.
 * Sert à dessiner l'ombre portée au sol.
 */
[[nodiscard]] MeshData disc(float radius, int segments, const vec3& color);

/*
 * Hélipad posé à plat (plan XZ, normale +Y), centré sur l'origine : un disque
 * sombre, un anneau près du bord et un grand H au centre. Sert à marquer la zone
 * de départ où l'appareil est posé et où le reset le ramène.
 */
[[nodiscard]] MeshData helipad(float radius, int segments, const vec3& padColor,
                               const vec3& ringColor, const vec3& letterColor);

}  /* namespace artouste::render::primitives */
