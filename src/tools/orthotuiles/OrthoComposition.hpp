/*
 * OrthoComposition.hpp
 * Image source en mémoire, et fabrication d'une tuile à partir d'elle.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/tuiles/Pyramide.hpp"

#include <vector>

namespace orthotuiles {

struct Source {
    std::vector<unsigned char> pixels;  /* 4 octets par pixel, rangée 0 au nord */
    int                        largeur = 0;
    int                        hauteur = 0;
};

/* Tuile rééchantillonnée depuis la source, à la finesse de la grille. */
void composerTuile(const Source&                          src,
                   const artouste::render::tuiles::Calage& calage,
                   float                                   srcPxParM,
                   int                                     col,
                   int                                     rangee,
                   std::vector<unsigned char>&             sortie);

/* Recopie directe : le bloc est déjà à la finesse cible et aligné sur la
   grille. */
void copierTuile(const Source&               src,
                 int                         tuilePx,
                 int                         dc,
                 int                         dr,
                 std::vector<unsigned char>& sortie);

/* Part de pixels sans donnée (blanc pur) : au-delà d'un seuil, la tuile n'est
   pas écrite et le moteur garde l'orthophoto d'ensemble. */
[[nodiscard]] float partBlanche(const std::vector<unsigned char>& tuile);

} /* namespace orthotuiles */
