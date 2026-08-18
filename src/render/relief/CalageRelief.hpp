/*
 * CalageRelief.hpp
 * Calage d'un jeu de tuiles de relief, et emboîtement de la fenêtre dans la
 * maille de la carte.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <cmath>

namespace artouste::render::relief {

/* Période commune des réseaux en jeu, en mailles : les pas des deux grilles et
   les blocs des niveaux lus. Le centre s'y cale, sans quoi un réseau glisse sur
   l'autre à chaque pas et les normales sautent.

   Pas le pas de la grille la plus grossière : l'anneau passé à 4 m, le centre
   alternerait entre deux phases du réseau de 8 m. */
/* La grille de la fenêtre s'emboîte-t-elle dans la maille de la carte ? Il le
   faut pour le noyau ET pour l'anneau, qui dessine à PAS_ANNEAU fois ce pas :
   sinon la fenêtre redessine la surface du maillage au lieu de la reproduire, et
   les silhouettes se déplacent à sa frontière. */
[[nodiscard]] inline bool emboiteDansMaille(float pasFenetre, float mailleCarte,
                                            int multAnneau) noexcept {
    if (pasFenetre <= 0.0f || mailleCarte <= 0.0f || multAnneau < 1) {
        return false;
    }
    const float k = mailleCarte / (pasFenetre * static_cast<float>(multAnneau));
    return k >= 1.0f && std::fabs(k - std::round(k)) < 1e-3f;
}

inline constexpr int NIVEAU_ANNEAU  = 2;   /* niveau le plus grossier que l'anneau lit */
inline constexpr int CALAGE_MAILLES = 1 << NIVEAU_ANNEAU;

/* Fondu du bord vers le relief d'ensemble. Il rattrape l'écart entre MNT LiDAR
   et RGE ALTI (4 m au sommet du Pic du Midi, 28 m sur une falaise), qui ferait
   sinon une marche.

   RADIAL et large, comme celui de la fenêtre d'image : un fondu carré et court
   dessine un angle droit en travers du paysage, vu en vol le 16/08 sur le
   versant du Pic du Midi. La proportion est celle de tuiles/Fenetre.cpp. */
inline constexpr float BORD_RETRAIT_M = 10.0f;
inline constexpr float BORD_PLEIN     = 0.62f;

/* Maille à laquelle on considère que la carte porte déjà le relief. Le lissage
   de la fenêtre à cette échelle est retranché d'elle-même : ne reste que le
   détail que la carte ne peut pas tenir. Valeur ronde proche de la maille d'une
   carte de montagne (17,5 m sur 18 km). */
inline constexpr float MAILLE_CARTE_M = 16.0f;

/* Calage de la grille de tuiles, en coordonnées monde. Comme tuiles::Calage,
   mais une tuile porte des points d'altitude, sans recouvrement entre tuiles. */
struct Calage {
    int   tuilePoints = 512;
    /* Pas PAR AXE. Il vaut dx/k et dz/k de la carte, k entier, si bien que la
       grille de la fenêtre s'emboîte exactement dans celle du maillage
       d'ensemble. Les deux axes de la carte n'ayant pas la même maille, ces deux
       pas diffèrent : une tuile n'est PAS carrée au sol. */
    float pasX        = 0.0f;
    float pasZ        = 0.0f;
    int   colonnes    = 0;
    int   rangees     = 0;
    float coinX       = 0.0f;
    float coinZ       = 0.0f;

    [[nodiscard]] float tuileX() const noexcept {
        return static_cast<float>(tuilePoints) * pasX;
    }
    [[nodiscard]] float tuileZ() const noexcept {
        return static_cast<float>(tuilePoints) * pasZ;
    }
    [[nodiscard]] bool valide() const noexcept {
        return tuilePoints >= 16 && pasX > 0.0f && pasZ > 0.0f && colonnes > 0 && rangees > 0;
    }
};

} /* namespace artouste::render::relief */
