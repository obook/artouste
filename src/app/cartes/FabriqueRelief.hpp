/*
 * FabriqueRelief.hpp
 * Fabrication du jeu de tuiles de RELIEF d'une carte : récupération du MNT
 * LiDAR HD auprès de l'IGN et découpage en tuiles .r16, ce que FabriqueTuiles
 * est à l'orthophoto. C'est ce qui donne à une carte la fenêtre de relief fin
 * sans passer par tools/terrain/fetch_relief.py.
 *
 * Le travail est celui de la fabrique d'images, en plus court : le service rend
 * des altitudes brutes en BIL 32 bits, il n'y a ni image à décoder ni
 * compression. Il passe par la MÊME fabrique (Fabrique::lancerRelief) : un seul
 * fil, un seul avancement, une seule fabrication à la fois.
 *
 * Le format des tuiles, celui de l'index et le témoin d'inachèvement sont ceux
 * de tools/terrain/relief_tuiles.py : les deux chemins doivent produire le même
 * jeu, et une fabrication commencée d'un côté doit se reprendre de l'autre.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/cartes/FabriqueTuiles.hpp"

#include <filesystem>

namespace artouste::app::cartes {

/* Côté d'une tuile de relief, en points d'altitude. Les points ne se recouvrent
   pas d'une tuile à l'autre : la fenêtre du moteur les lit comme une grille
   continue (voir render/relief/FenetreRelief.hpp). */
inline constexpr int RELIEF_TUILE_POINTS = 512;

/* Tuiles par côté de bloc, et suréchantillonnage avant moyenne. Deux tuiles de
   512 points suréchantillonnées deux fois font 2048 pixels par requête, sous la
   limite de ~5000 px du service. Le MNT LiDAR est natif à 1 m : deux pixels par
   point à 2 m les moyennent exactement. */
inline constexpr int RELIEF_TUILES_PAR_BLOC = 2;
inline constexpr int RELIEF_SUR_ECH         = 2;

/* Pas visé, en mètres. Le pas retenu est celui qui s'en approche le plus PARMI
   ceux qui s'emboîtent dans la maille de la carte (voir grilleRelief). */
inline constexpr float RELIEF_PAS_VISE_M = 2.0f;

/* Un point du laser qui plonge de plus de cela sous le relief en place ne mesure
   pas le sol : 421 points sur ossau, jusqu'à -796 m sur un versant à 2400 m.
   Même garde-fou que fetch_relief.py. */
inline constexpr float CHUTE_ABERRANTE_M = 300.0f;

/* Sentinelle du service pour un point sans donnée ; tout ce qui est en dessous
   est un trou (même valeur que tools/terrain/config.py). */
inline constexpr float RELIEF_NODATA = -1000.0f;

/* Grille de tuiles de relief : celle que la carte demande (grilleRelief) ou
   celle qu'un jeu déjà posé annonce (lireIndexRelief). Les deux se comparent
   pour savoir si une reprise est possible. */
struct GrilleRelief {
    float pasX     = 0.0f;
    float pasZ     = 0.0f;
    int   colonnes = 0;
    int   rangees  = 0;
    bool  valide   = false;

    [[nodiscard]] int tuiles() const noexcept { return colonnes * rangees; }
};

/* Grille que cette carte demande : pas par axe, et nombre de tuiles couvrant
   l'emprise. Le pas n'est pas libre. Il vaut la maille du maillage d'ensemble
   divisée par un multiple de PAS_ANNEAU, sans quoi la fenêtre redessine la
   surface au lieu de la reproduire et sa frontière se voit en vol. Invalide si
   le terrain.txt ne donne pas son maillage. */
[[nodiscard]] GrilleRelief grilleRelief(const std::filesystem::path& dossierCarte);

/* Grille annoncée par l'index d'un jeu en place. Invalide si le dossier n'en a
   pas ou s'il est illisible. */
[[nodiscard]] GrilleRelief lireIndexRelief(const std::filesystem::path& dossierRelief);

/* Ce que coûterait la fabrication du relief de cette carte : place occupée,
   téléchargement, durée probable. Deux octets par point sur le disque, seize
   sur la ligne, le suréchantillonnage étant reçu en 32 bits. */
[[nodiscard]] Estimation estimerRelief(const std::filesystem::path& dossierCarte);

} /* namespace artouste::app::cartes */
