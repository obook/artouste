/*
 * OrthoCalage.hpp
 * Calage d'une carte et grille de tuiles qui la couvre.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/tuiles/Pyramide.hpp"

#include <filesystem>

namespace orthotuiles {

struct CalageCarte {
    float largeurM = 0.0f;
    float hauteurM = 0.0f;
    float originX  = 0.0f;
    float originZ  = 0.0f;
};

/* Lit le terrain.txt de la carte. */
bool lireCalageCarte(const std::filesystem::path& terrainTxt, CalageCarte& out);

/* Grille ancrée sur le coin nord-ouest de la carte, couvrant toute l'emprise ;
   la dernière tuile de chaque rangée et de chaque colonne dépasse au besoin.
   Ne dépend jamais de l'image fournie : c'est ce qui permet de remplir la même
   grille par blocs successifs. */
[[nodiscard]] artouste::render::tuiles::Calage grilleDeCarte(const CalageCarte& carte,
                                                             int                tuilePx,
                                                             float              mParPixel);

} /* namespace orthotuiles */
