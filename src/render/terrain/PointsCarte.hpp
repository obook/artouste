/*
 * PointsCarte.hpp
 * Points remarquables d'une carte (lieux, hélipads, monuments 3D) et lecture
 * d'une altitude entre quatre sommets de la grille.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <string>
#include <vector>

namespace artouste::render {

/* Interpolation dans une maille de la carte, SUIVANT SES TRIANGLES. La maille
   est coupée par l'anti-diagonale, du coin (i+1, j) au coin (i, j+1) : c'est la
   découpe du maillage d'ensemble (voir la construction des indices) et celle de
   la grille de la fenêtre (construireGrille). Les trois doivent la partager,
   sinon la fenêtre et le maillage ne dessinent pas la même surface.

   Une bilinéaire à sa place passerait SOUS les triangles sur les fortes pentes,
   et l'appareil posé s'y enfoncerait. */
[[nodiscard]] inline float interpolerTriangle(float h00, float h10, float h01, float h11,
                                              float tx, float tz) noexcept {
    if (tx + tz <= 1.0f) {
        return h00 + tx * (h10 - h00) + tz * (h01 - h00);
    }
    return h11 + (1.0f - tx) * (h01 - h11) + (1.0f - tz) * (h10 - h11);
}

/* Lieu remarquable du terrain (nom + position WGS84) : étiqueté sur la scène et
   pointé sur la minimap. Chaque terrain a ses propres lieux, lus de landmarks.txt
   dans son dossier. */
struct Landmark {
    std::string name;
    float lon = 0.0f;
    float lat = 0.0f;
    /* Cap du marquage, en degrés (0 = nord), facultatif : il oriente l'axe des
       montants du H d'un hélipad, que le pilote suit à l'approche. Zéro, la valeur
       par défaut, laisse le H montants nord-sud, comme avant l'existence du champ.
       Ignoré pour un lieu remarquable, qui n'a pas de marquage au sol. */
    float headingDeg = 0.0f;
};

/* Monument 3D posé sur la carte : un modèle ponctuel placé à une coordonnée
   WGS84, lu de monuments.txt dans le dossier du terrain. Les bâtiments extrudés
   de la BD TOPO décrivent déjà l'emprise de ces édifices ; clearRadiusM dit
   quel rayon leur laisser libre pour ne pas empiler deux géométries. */
struct Monument {
    /* Chemin du modèle SOUS assets/models/monuments/, rangé par jeu d'origine
       (paris/TourEiffel-ba.ac). Toujours relatif et sans remontée : voir le
       filtrage de loadMonuments. */
    std::string file;
    std::string name;
    float lon = 0.0f;
    float lat = 0.0f;
    /* Altitude du socle en mètres. Le relief de Paris tient une cellule tous les
       17,6 m, trop lâche pour caler proprement un socle : une altitude explicite
       vaut mieux que heightAt. onGround remet le modèle sur le relief quand le
       fichier dit "sol" plutôt qu'un nombre. */
    float altitudeM = 0.0f;
    bool  onGround  = false;
    float headingDeg = 0.0f; /* cap boussole (0 = nord, 90 = est) */
    /* Échelles horizontale et verticale, séparées. Les modèles FlightGear sont
       en mètres (1 par défaut), mais ils ne sont pas toujours aux proportions de
       l'édifice : celui de la tour Eiffel est régulièrement tassé de 11 % en
       hauteur pour 3 % en largeur, ce qu'un facteur unique ne peut pas
       rattraper. Le rapport de la texture s'en trouve étiré d'autant, ce qui ne
       se voit pas sur un treillis. */
    float scaleH = 1.0f;
    float scaleV = 1.0f;
    float clearRadiusM = 0.0f;
};

} /* namespace artouste::render */
