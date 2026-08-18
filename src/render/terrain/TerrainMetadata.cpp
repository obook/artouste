/*
 * TerrainMetadata.cpp
 * Lecture de terrain.txt (voir TerrainInterne.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/terrain/TerrainInterne.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace artouste::render {

/*
 * Lit le fichier de calage terrain.txt (lignes "clé valeur", # = commentaire)
 * et range les valeurs attendues. Renvoie faux si une clé manque.
 */
bool readMetadata(const std::filesystem::path& path,
                  int& cols,
                  int& rows,
                  float& widthM,
                  float& heightM,
                  float& elevMin,
                  float& elevMax,
                  bool& drawSea,
                  bool& hasStart,
                  float& startX,
                  float& startZ,
                  float& startHeadingDeg,
                  bool& hasGeo,
                  float& lonMin,
                  float& lonMax,
                  float& latMin,
                  float& latMax,
                  float& originX,
                  float& originZ) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    originX = 0.0f; /* facultatifs : 0 = emprise centrée sur l'origine du monde */
    originZ = 0.0f;
    bool hasCols = false, hasRows = false, hasW = false, hasH = false, hasMin = false,
         hasMax = false;
    bool hasStartX = false, hasStartZ = false;
    bool hasLonMin = false, hasLonMax = false, hasLatMin = false, hasLatMax = false;
    std::string key;
    while (file >> key) {
        if (!key.empty() && key[0] == '#') {
            std::getline(file, key); /* on jette le reste de la ligne de commentaire */
            continue;
        }
        if (key == "cols") {
            file >> cols, hasCols = true;
        } else if (key == "rows") {
            file >> rows, hasRows = true;
        } else if (key == "width_m") {
            file >> widthM, hasW = true;
        } else if (key == "height_m") {
            file >> heightM, hasH = true;
        } else if (key == "elev_min") {
            file >> elevMin, hasMin = true;
        } else if (key == "elev_max") {
            file >> elevMax, hasMax = true;
        } else if (key == "sea") { /* 0 = pas de plan de mer (terrain de montagne) */
            int v = 1;
            file >> v;
            drawSea = (v != 0);
        } else if (key == "start_x") {
            file >> startX, hasStartX = true;
        } else if (key == "start_z") {
            file >> startZ, hasStartZ = true;
        } else if (key == "start_heading") { /* cap initial (deg boussole), facultatif */
            file >> startHeadingDeg;
        } else if (key == "origin_x") { /* décalage d'origine (carte recadrée), facultatif */
            file >> originX;
        } else if (key == "origin_z") {
            file >> originZ;
        } else if (key == "lon_min") {
            file >> lonMin, hasLonMin = true;
        } else if (key == "lon_max") {
            file >> lonMax, hasLonMax = true;
        } else if (key == "lat_min") {
            file >> latMin, hasLatMin = true;
        } else if (key == "lat_max") {
            file >> latMax, hasLatMax = true;
        } else {
            std::getline(file, key); /* clé ignorée : on saute sa valeur */
        }
    }
    hasStart = hasStartX && hasStartZ;
    hasGeo = hasLonMin && hasLonMax && hasLatMin && hasLatMax;
    return hasCols && hasRows && hasW && hasH && hasMin && hasMax;
}

} /* namespace artouste::render */
