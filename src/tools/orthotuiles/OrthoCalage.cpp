/*
 * OrthoCalage.cpp
 * Calage d'une carte et grille de tuiles (voir OrthoCalage.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "tools/orthotuiles/OrthoCalage.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

namespace orthotuiles {

using artouste::render::tuiles::Calage;

bool lireCalageCarte(const std::filesystem::path& terrainTxt, CalageCarte& out) {
    std::ifstream in(terrainTxt);
    if (!in) {
        return false;
    }
    bool        aLargeur = false, aHauteur = false;
    std::string cle;
    while (in >> cle) {
        if (!cle.empty() && cle[0] == '#') {
            std::getline(in, cle);
            continue;
        }
        if (cle == "width_m") {
            in >> out.largeurM, aLargeur = true;
        } else if (cle == "height_m") {
            in >> out.hauteurM, aHauteur = true;
        } else if (cle == "origin_x") {
            in >> out.originX;
        } else if (cle == "origin_z") {
            in >> out.originZ;
        } else {
            std::getline(in, cle);
        }
    }
    return aLargeur && aHauteur && out.largeurM > 0.0f && out.hauteurM > 0.0f;
}

artouste::render::tuiles::Calage grilleDeCarte(const CalageCarte& carte,
                                               int                tuilePx,
                                               float              mParPixel) {
    artouste::render::tuiles::Calage c;
    c.tuilePx   = tuilePx;
    c.mParPixel = mParPixel;
    const float tuileM = static_cast<float>(tuilePx) * mParPixel;
    c.colonnes         = static_cast<int>(std::ceil(carte.largeurM / tuileM));
    c.rangees          = static_cast<int>(std::ceil(carte.hauteurM / tuileM));
    c.coinX            = carte.originX - 0.5f * carte.largeurM;
    c.coinZ            = carte.originZ - 0.5f * carte.hauteurM;
    return c;
}

} /* namespace orthotuiles */
