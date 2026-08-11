/*
 * VegetationMasks.cpp
 * Zones d'exclusion (exclusions.txt) qui écartent des cercles entiers du
 * semis de végétation (aérodromes, etc.). Le masque d'eau est dans
 * VegetationWaterMask.cpp, le masque des bâtiments dans
 * VegetationBuildingMask.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"
#include "render/Vegetation.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace artouste::render {

namespace {

/* Dégagement imposé autour de chaque aire de poser : aucun arbre à moins de
   50 m d'un pad, sur toutes les cartes, que la carte ait ou non son
   exclusions.txt. Un arbre planté sur le H empêche de s'y poser. */
constexpr float PAD_DEGAGEMENT_M = 50.0f;

} /* namespace */

std::vector<Vegetation::Exclusion>
Vegetation::loadExclusions(const std::filesystem::path& terrainDir, const Terrain& terrain) const {
    /* Zones d'exclusion (aérodromes, etc.) : cercles "lon lat rayon_m" lus depuis
       exclusions.txt, où l'on ne plante aucun arbre. L'orthophoto y est verte
       (pistes et bandes enherbées) et passerait le test de forêt : on les écarte
       explicitement. Chaque cercle est converti une fois en coordonnées monde
       (centre + rayon au carré), comme le dégagement de départ et les lacs de
       secours. Fichier optionnel : absent, aucune exclusion. */
    std::vector<Exclusion> exclusions;
    std::ifstream f(terrainDir / "exclusions.txt");
    std::string line;
    while (std::getline(f, line)) {
        const std::size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#') {
            continue; /* ligne vide ou commentaire */
        }
        float lon = 0.0f, lat = 0.0f, radius = 0.0f;
        std::istringstream iss(line);
        if (!(iss >> lon >> lat >> radius) || radius <= 0.0f) {
            continue;
        }
        /* worldAt() renvoie des coordonnées MONDE ; le semis (VegetationScatter.cpp)
           compare ces cercles à des coordonnées LOCALES : conversion nécessaire sur
           une carte recadrée (originX/originZ non nuls), sinon l'exclusion (piste
           d'aérodrome, etc.) tombe à côté et n'écarte pas les bons arbres. */
        float ex = 0.0f, ez = 0.0f;
        terrain.worldAt(lon, lat, ex, ez);
        ex -= terrain.originX();
        ez -= terrain.originZ();
        exclusions.push_back({ex, ez, radius * radius});
    }

    /* Aires de poser (helipads.txt) : même traitement, avec un rayon imposé. Le
       semis ne connaissait auparavant que le repère de départ de terrain.txt, qui
       ne sert qu'à CHOISIR l'hélipad de départ (voir ApplicationScene) et s'en
       écarte de plus de 100 m sur la plupart des cartes : le dégagement tombait
       donc à côté du pad, et les pads autres que celui de départ n'en avaient
       aucun. */
    for (const Landmark& pad : terrain.helipads()) {
        float px = 0.0f, pz = 0.0f;
        terrain.worldAt(pad.lon, pad.lat, px, pz);
        exclusions.push_back({px - terrain.originX(), pz - terrain.originZ(),
                              PAD_DEGAGEMENT_M * PAD_DEGAGEMENT_M});
    }
    return exclusions;
}

} /* namespace artouste::render */
