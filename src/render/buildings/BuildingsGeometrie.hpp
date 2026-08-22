/*
 * BuildingsGeometrie.hpp
 * Extrusion d'une emprise en un volume simple : murs verticaux et toit plat,
 * et extrusion d'un axe d'ouvrage d'art en tablier.
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

/* Épaisseur donnée au tablier d'un ouvrage d'art, en mètres. La BD TOPO donne
   l'altitude de la CHAUSSÉE ; le dessous est posé à cette distance en dessous,
   faute de mesure du gabarit réel. */
inline constexpr float EPAISSEUR_TABLIER_M = 1.2f;

/* Drapage de l'orthophoto de la carte : de quoi calculer l'UV d'un point du
   monde exactement comme le fait le maillage du terrain (TerrainMaillage.cpp).
   La chaussée d'un tablier s'en sert pour porter la photo du pont lui-même,
   au lieu d'un gris uni. */
struct CalageOrtho {
    float coinX   = 0.0f; /* coin nord-ouest de l'emprise, en monde */
    float coinZ   = 0.0f;
    float tailleX = 1.0f; /* emprise au sol */
    float tailleZ = 1.0f;

    [[nodiscard]] vec2 uv(float x, float z) const {
        return vec2{(x - coinX) / tailleX, 1.0f - (z - coinZ) / tailleZ};
    }
};

/* Un ouvrage d'art : un ruban plat de largeur donnée, suivant l'axe (px, py,
   pz) point par point, avec dessus, dessous et deux flancs. Les altitudes py
   viennent de la BD TOPO et non du relief : c'est tout l'intérêt, le modèle de
   terrain ne connaît pas les ponts.

   La géométrie sort en DEUX flots, parce qu'ils ne se dessinent pas pareil :

   - beton  : dessous et flancs, à draper de rien du tout, dessinés avec les
              bâtiments (couleur unie) ;
   - chaussee : la face du dessus, dessinée avec le TERRAIN, qui lui pose
              l'orthophoto et les tuiles de détail par coordonnées monde. Le
              tablier montre alors la photo du pont, à la finesse du sol
              d'à-côté, au lieu d'un aplat gris.

   Les sommets et les indices sont ajoutés aux tampons donnés. */
void extruderTablier(std::vector<Vertex>&       vertsBeton,
                     std::vector<unsigned int>& idxBeton,
                     std::vector<Vertex>&       vertsChaussee,
                     std::vector<unsigned int>& idxChaussee,
                     const std::vector<float>&  px,
                     const std::vector<float>&  py,
                     const std::vector<float>&  pz,
                     float                      largeur,
                     const vec3&                couleur,
                     const CalageOrtho&         ortho);

} /* namespace artouste::render */
