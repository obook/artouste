/*
 * BuildingsGeometrie.hpp
 * Extrusion d'une emprise en un volume simple : murs verticaux et toit plat.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Mesh.hpp"
#include "util/Math.hpp"

#include <vector>

namespace artouste::render {

/* Un bâtiment : un quad par côté de l'emprise, plus un toit plat en éventail
   depuis le premier sommet (correct pour une emprise convexe, acceptable de
   loin pour les rares formes concaves).
   Les sommets et les indices sont ajoutés aux tampons donnés. */
void extruderBatiment(std::vector<Vertex>&       verts,
                      std::vector<unsigned int>& idx,
                      const std::vector<float>&  px,
                      const std::vector<float>&  pz,
                      float                      cx,
                      float                      cz,
                      float                      base,
                      float                      hauteur,
                      const vec3&                mur,
                      const vec3&                toit);

} /* namespace artouste::render */
