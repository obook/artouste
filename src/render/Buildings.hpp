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

#include <cstddef>
#include <filesystem>

namespace artouste::render {

class Terrain;  /* géoréférencement (lon/lat -> monde) et altitude du sol */

class Buildings {
public:
    /* Charge buildings.bin depuis dir et construit le maillage extrudé, calé sur
       le relief de terrain. Fichier absent : maillage vide (built() == false). */
    Buildings(const std::filesystem::path& dir, const Terrain& terrain);

    void draw() const { m_mesh.draw(); }

    /* Vrai si au moins un bâtiment a été chargé et extrudé. */
    [[nodiscard]] bool built() const noexcept { return m_count > 0; }

    /* Nombre de bâtiments extrudés. */
    [[nodiscard]] std::size_t count() const noexcept { return m_count; }

private:
    Mesh        m_mesh;
    std::size_t m_count = 0;
};

}  /* namespace artouste::render */
