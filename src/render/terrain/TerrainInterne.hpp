/*
 * TerrainInterne.hpp
 * Ce que les fichiers du terrain se passent entre eux et qui n'a pas à sortir
 * de render/terrain/.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <filesystem>

namespace artouste::render {

/* Lit le fichier de calage terrain.txt (lignes "clé valeur", # = commentaire)
   et range les valeurs attendues. Faux si une clé indispensable manque. */
bool readMetadata(const std::filesystem::path& path,
                  int&                         cols,
                  int&                         rows,
                  float&                       widthM,
                  float&                       heightM,
                  float&                       elevMin,
                  float&                       elevMax,
                  bool&                        drawSea,
                  bool&                        hasStart,
                  float&                       startX,
                  float&                       startZ,
                  float&                       startHeadingDeg,
                  bool&                        hasGeo,
                  float&                       lonMin,
                  float&                       lonMax,
                  float&                       latMin,
                  float&                       latMax,
                  float&                       originX,
                  float&                       originZ);

} /* namespace artouste::render */
