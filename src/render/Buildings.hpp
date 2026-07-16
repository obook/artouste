/*
 * Buildings.hpp
 * Bâtiments 3D du terrain : emprises au sol issues de la BD TOPO de l'IGN
 * (fichier buildings.bin produit par tools/fetch_buildings.py), extrudées en
 * volumes simples (murs verticaux + toit plat) à leur hauteur réelle. Tout est
 * fusionné en un seul maillage statique, posé sur le relief. Si le fichier est
 * absent, le maillage reste vide et rien n'est dessiné.
 *
 * Repère monde : X vers l'est, Z vers le sud, Y vers le haut.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Mesh.hpp"
#include "util/Math.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace artouste::render {

class Terrain;  /* géoréférencement (lon/lat -> monde) et altitude du sol */

class Buildings {
public:
    /* Charge buildings.bin depuis dir et construit le maillage extrudé, calé sur
       le relief de terrain. Fichier absent : maillage vide (built() == false).
       Le maillage est découpé en tuiles spatiales pour permettre le culling. */
    Buildings(const std::filesystem::path& dir, const Terrain& terrain);

    /* Dessine les bâtiments visibles uniquement. worldViewProj est la matrice
       monde -> clip (proj * vue monde, sans le recalage d'origine, qui s'annule) :
       on en extrait le frustum pour écarter les tuiles hors champ. camWorldPos sert
       au culling par distance : les tuiles entièrement au-delà de la brume (invisibles)
       ne sont pas dessinées. Le shader de bâtiments doit être actif et ses uniformes
       (dont u_model = recalage d'origine) déjà renseignés. */
    void draw(const mat4& worldViewProj, const vec3& camWorldPos) const;

    /* Vrai si au moins un bâtiment a été chargé et extrudé. */
    [[nodiscard]] bool built() const noexcept { return m_count > 0; }

    /* Nombre de bâtiments extrudés. */
    [[nodiscard]] std::size_t count() const noexcept { return m_count; }

private:
    /* Tuile spatiale : une plage d'indices contiguë dans le maillage unique et sa
       boîte englobante monde (min/max), pour le test de visibilité. */
    struct Tile {
        int  firstIndex;  /* premier indice de la plage */
        int  indexCount;  /* nombre d'indices de la plage */
        vec3 mn;          /* coin min de la boîte englobante (monde) */
        vec3 mx;          /* coin max de la boîte englobante (monde) */
    };

    Mesh              m_mesh;
    std::vector<Tile> m_tiles;
    std::size_t       m_count = 0;
};

}  /* namespace artouste::render */
