/*
 * FabriqueInterne.hpp
 * Ce que les fichiers de la fabrique de tuiles se passent entre eux.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/tuiles/Pyramide.hpp"

#ifdef ARTOUSTE_HAS_CURL
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif
#include <curl/curl.h>
#endif

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace artouste::app::cartes {

/* Service d'orthophotos de la Géoplateforme IGN (BD ORTHO), le même que celui
   des scripts de préparation des cartes (tools/terrain/config.py). Données sous
   Licence Ouverte Etalab 2.0. */
constexpr const char* WMS_URL   = "https://data.geopf.fr/wms-r/wms";
constexpr const char* WMS_LAYER = "ORTHOIMAGERY.ORTHOPHOTOS";

/* Une tuile de 512 px pèse environ 350 Ko en BC7 ; le bloc JPEG qui la produit
   est bien plus léger. Sert à annoncer un ordre de grandeur du téléchargement
   avant d'avoir mesuré quoi que ce soit. */
constexpr double OCTETS_JPEG_PAR_PIXEL = 0.25;

/* Cadence de fabrication, tuiles par seconde, découpage et compression BC7
   compris. Mesurée en cours de route sur une fabrication réelle : 4,3 à 4,5
   tuiles par seconde, deux relevés concordants. On retient 4,0, un peu en
   dessous, pour ne pas promettre plus court que nature.

   Ne pas la déduire de l'horodatage d'un jeu terminé : l'écart entre la première
   et la dernière tuile inclut les pauses et les interruptions, ce qui donnait
   1,8 et faisait annoncer le double du temps réel. Cette valeur dépend de la
   machine et n'est qu'un ordre de grandeur ; la mesure du fil de fabrication
   prend le relais dès le premier bloc terminé. */
constexpr double TUILES_PAR_SECONDE = 4.0;

/* Calage d'une carte, lu dans son terrain.txt. */
struct CalageCarte {
    float largeurM = 0.0f;
    float hauteurM = 0.0f;
    float originX  = 0.0f;
    float originZ  = 0.0f;
    float lonMin = 0.0f, lonMax = 0.0f, latMin = 0.0f, latMax = 0.0f;
    /* Hauteur en pixels de l'orthophoto d'ensemble. C'est la seule des deux
       dimensions qui compte : la finesse au sol est l'étendue nord-sud divisée
       par elle, comme la mesure Terrain::orthoMetersPerPixel. Zéro si le fichier
       ne la donne pas, ce qui n'invalide pas le calage. */
    int   orthoHauteur = 0;
    bool  valide = false;
};

/* Lit le terrain.txt de la carte. Un calage sans largeur est invalide. */
[[nodiscard]] CalageCarte lireCalage(const std::filesystem::path& dossierCarte);

/* Grille de tuiles couvrant la carte à la finesse demandée. */
[[nodiscard]] render::tuiles::Calage grille(const CalageCarte& carte, float mParPixel);

/* Taille lisible, comme dans le gestionnaire de cartes. */
[[nodiscard]] std::string formaterOctets(std::uintmax_t octets);

#ifdef ARTOUSTE_HAS_CURL

/* Demande un bloc d'orthophoto au WMS de l'IGN. */
[[nodiscard]] bool demanderBloc(CURL*                       curl,
                                double                      latLo,
                                double                      lonLo,
                                double                      latHi,
                                double                      lonHi,
                                int                         largeur,
                                int                         hauteur,
                                std::vector<unsigned char>& sortie);

/* Part de pixels sans donnée (blanc pur) dans une tuile : au-delà d'un seuil on
   n'écrit pas la tuile et le moteur garde l'orthophoto d'ensemble. */
[[nodiscard]] float partBlanche(const std::vector<unsigned char>& tuile);

#endif /* ARTOUSTE_HAS_CURL */

} /* namespace artouste::app::cartes */
