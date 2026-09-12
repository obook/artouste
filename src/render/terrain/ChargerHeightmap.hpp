/*
 * ChargerHeightmap.hpp
 * Le relief d'ensemble d'une carte, lu par le moteur et par la fabrique.
 *
 * Auteur : O. Booklage
 * Date : septembre 2026
 * Licence : GPL v2
 */

#pragma once

#include <filesystem>
#include <vector>

namespace artouste::render {

/* Altitudes en mètres, rangée 0 au nord. heightmap.bin d'abord, sinon
   heightmap.png étalé entre elevMin et elevMax. Un .bin de mauvaise taille
   échoue au lieu de retomber sur le png, qui cacherait le défaut.
   Vide si rien n'est lisible, la cause allant sur stderr.

   Le .bin : cols x rows flottants de quatre octets, sans en-tête, boutisme
   natif -- petit-boutiste sur x86-64 et arm64, les seules cibles. Pas de
   valeur manquante admise (le -99999 ou NaN du BIL est à nettoyer avant
   d'écrire). */
[[nodiscard]] std::vector<float> chargerHeightmap(const std::filesystem::path& dossierCarte,
                                                  int cols, int rows, float elevMin,
                                                  float elevMax);

} /* namespace artouste::render */
