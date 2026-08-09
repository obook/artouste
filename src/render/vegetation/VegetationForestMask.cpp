/*
 * VegetationForestMask.cpp
 * Masque de forêt (forest.png, fabriqué par tools/fetch_forest.py à partir des
 * données IGN) : il dit où la forêt est cartographiée, et par quelle essence
 * dominante. Le semis s'en sert à la place du seul test de couleur de
 * l'orthophoto, qui plantait sur une pelouse sombre ou une ombre de versant et
 * oubliait une forêt en plein soleil. Le masque d'eau est dans
 * VegetationWaterMask.cpp, celui des bâtiments dans VegetationBuildingMask.cpp.
 *
 * Le fichier couvre exactement la même emprise que l'orthophoto, mais à sa
 * propre résolution (10 m/px suffisent : la BD Forêt ne descend pas sous 0,5 ha).
 * On le garde donc tel quel, sans rééchantillonnage : le semis l'interroge avec
 * toPixel() en lui passant les dimensions du masque.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/Vegetation.hpp"

#include <stb_image.h>

#include <cstdio>

namespace artouste::render {

std::vector<unsigned char> Vegetation::buildForestMask(const std::filesystem::path& terrainDir,
                                                       int& forestW,
                                                       int& forestH) const {
    /* Niveaux de gris écrits par tools/fetch_forest.py (voir son en-tête) :
       0 hors de France, 40 en France et non boisé, 85 feuillus, 130 pins,
       170 autres conifères, 255 mélange. On les traduit ici en codes Forest*
       pour que le semis n'ait pas à connaître ces valeurs. */
    forestW = 0;
    forestH = 0;
    std::vector<unsigned char> forest;
    int            channels = 0;
    unsigned char* png = stbi_load((terrainDir / "forest.png").string().c_str(), &forestW,
                                   &forestH, &channels, 1);
    if (png == nullptr) {
        /* Carte sans masque (pas encore fabriqué, ou carte recadrée) : le semis
           retombe entièrement sur le test de couleur, comme avant. */
        forestW = 0;
        forestH = 0;
        return forest;
    }

    forest.resize(static_cast<std::size_t>(forestW) * static_cast<std::size_t>(forestH));
    for (std::size_t i = 0; i < forest.size(); ++i) {
        const unsigned char v = png[i];
        if (v < 20) {
            forest[i] = ForestHorsFrance;
        } else if (v < 60) {
            forest[i] = ForestNonBoise;
        } else if (v < 110) {
            forest[i] = ForestFeuillu;
        } else if (v < 150) {
            forest[i] = ForestPin;
        } else if (v < 213) {
            forest[i] = ForestConifere;
        } else {
            forest[i] = ForestMixte;
        }
    }
    stbi_image_free(png);

    std::printf("[Vegetation] masque BD Forêt %dx%d chargé.\n", forestW, forestH);
    return forest;
}

} /* namespace artouste::render */
