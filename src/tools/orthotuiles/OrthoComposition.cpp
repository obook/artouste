/*
 * OrthoComposition.cpp
 * Fabrication d'une tuile depuis l'image source (voir OrthoComposition.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "tools/orthotuiles/OrthoComposition.hpp"

#include <algorithm>
#include <cmath>

namespace orthotuiles {

using artouste::render::tuiles::Calage;

namespace {

/* Un pixel de la tuile cible couvre en général plusieurs pixels de la source :
   on moyenne son empreinte au lieu d'y prendre un seul échantillon, sinon le
   sous-échantillonnage crénèle les toits et les marquages au sol. Quand
   l'empreinte est plus petite qu'un pixel source (finesse cible proche de la
   source), la moyenne dégénère naturellement en une lecture du pixel couvrant.
   Les bords sont pincés sur l'image : la dernière tuile d'une rangée dépasse
   l'emprise de la carte quand la largeur n'est pas un multiple entier du côté
   de tuile, et prolonge alors la dernière colonne de pixels. */
void moyenne(const Source& src,
             float         x0,
             float         y0,
             float         x1,
             float         y1,
             unsigned char* sortie) {
    const int ix0 = std::clamp(static_cast<int>(std::floor(x0)), 0, src.largeur - 1);
    const int iy0 = std::clamp(static_cast<int>(std::floor(y0)), 0, src.hauteur - 1);
    const int ix1 = std::clamp(static_cast<int>(std::ceil(x1)) - 1, ix0, src.largeur - 1);
    const int iy1 = std::clamp(static_cast<int>(std::ceil(y1)) - 1, iy0, src.hauteur - 1);

    unsigned int somme[3] = {0, 0, 0};
    unsigned int nb       = 0;
    for (int y = iy0; y <= iy1; ++y) {
        const std::size_t ligne = static_cast<std::size_t>(y) * static_cast<std::size_t>(src.largeur);
        for (int x = ix0; x <= ix1; ++x) {
            const std::size_t i = (ligne + static_cast<std::size_t>(x)) * 4;
            somme[0] += src.pixels[i + 0];
            somme[1] += src.pixels[i + 1];
            somme[2] += src.pixels[i + 2];
            ++nb;
        }
    }
    if (nb == 0) {
        nb = 1;
    }
    sortie[0] = static_cast<unsigned char>(somme[0] / nb);
    sortie[1] = static_cast<unsigned char>(somme[1] / nb);
    sortie[2] = static_cast<unsigned char>(somme[2] / nb);
    sortie[3] = 255;
}

} /* namespace */

/* Remplit une tuile RGBA depuis la source. (col, rangee) situent la tuile dans
   la grille ; l'échelle passe des pixels de tuile aux pixels de source. */
void composerTuile(const Source&  src,
                   const Calage&  calage,
                   float          srcPxParM,
                   int            col,
                   int            rangee,
                   std::vector<unsigned char>& tuile) {
    const float pxParM = srcPxParM;
    const float tuileM = calage.tuileM();
    /* Coin nord-ouest de la tuile, en pixels de la source. La grille est ancrée
       sur le coin nord-ouest de la carte, qui est le pixel (0, 0) de la source. */
    const float baseX = static_cast<float>(col) * tuileM * pxParM;
    const float baseY = static_cast<float>(rangee) * tuileM * pxParM;
    const float pas   = calage.mParPixel * pxParM;  /* pixels source par pixel de tuile */

    for (int y = 0; y < calage.tuilePx; ++y) {
        const float sy0 = baseY + static_cast<float>(y) * pas;
        for (int x = 0; x < calage.tuilePx; ++x) {
            const float   sx0 = baseX + static_cast<float>(x) * pas;
            const std::size_t i =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(calage.tuilePx) +
                 static_cast<std::size_t>(x)) *
                4;
            moyenne(src, sx0, sy0, sx0 + pas, sy0 + pas, tuile.data() + i);
        }
    }
}

/* Mode bloc : la source est déjà à la finesse cible et alignée sur la grille,
   une tuile y occupe donc exactement tuilePx pixels. Simple recopie, sans
   rééchantillonnage : c'est ce qui permet de découper une carte fine que sa
   mosaïque complète rendrait impossible à tenir en mémoire. */
void copierTuile(const Source& src,
                 int           tuilePx,
                 int           colLocale,
                 int           rangeeLocale,
                 std::vector<unsigned char>& tuile) {
    for (int y = 0; y < tuilePx; ++y) {
        const int sy = rangeeLocale * tuilePx + y;
        for (int x = 0; x < tuilePx; ++x) {
            const int         sx = colLocale * tuilePx + x;
            const std::size_t i =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(tuilePx) +
                 static_cast<std::size_t>(x)) * 4;
            const std::size_t j = (static_cast<std::size_t>(sy) *
                                       static_cast<std::size_t>(src.largeur) +
                                   static_cast<std::size_t>(sx)) * 4;
            tuile[i + 0] = src.pixels[j + 0];
            tuile[i + 1] = src.pixels[j + 1];
            tuile[i + 2] = src.pixels[j + 2];
            tuile[i + 3] = 255;
        }
    }
}

/* Part de pixels de "no-data" de la BD ORTHO dans une tuile : le service IGN
   rend du blanc pur là où la couverture s'arrête, typiquement au-delà de la
   frontière espagnole sur une carte de montagne. L'orthophoto d'ensemble, elle,
   a été recousue à la préparation de la carte (voir tools/terrain/ortho.py,
   fill_nodata) : mieux vaut donc ne pas écrire une tuile blanche et laisser le
   moteur retomber sur cette orthophoto, plutôt que de plaquer un carré blanc
   sur le paysage. */
[[nodiscard]] float partBlanche(const std::vector<unsigned char>& tuile) {
    std::size_t blancs = 0;
    const std::size_t pixels = tuile.size() / 4;
    for (std::size_t i = 0; i < pixels; ++i) {
        if (tuile[i * 4 + 0] >= 248 && tuile[i * 4 + 1] >= 248 && tuile[i * 4 + 2] >= 248) {
            ++blancs;
        }
    }
    return (pixels == 0) ? 0.0f : static_cast<float>(blancs) / static_cast<float>(pixels);
}

} /* namespace orthotuiles */
