/*
 * FabriqueReliefInterne.hpp
 * Ce que les fichiers de la fabrique de relief se passent entre eux.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/cartes/FabriqueRelief.hpp"
#include "app/cartes/fabrique/FabriqueInterne.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace artouste::app::cartes {

/* Modèle numérique de TERRAIN du LiDAR HD : le sol NU. Le modèle de surface
   mettrait les toits et les cimes dans le relief, et l'on se poserait dessus.
   Servi par le même WMS que l'orthophoto (voir tools/lidar/services.py). */
constexpr const char* WMS_COUCHE_MNT =
    "IGNF_LIDAR-HD_MNT_ELEVATION.ELEVATIONGRIDCOVERAGE.WGS84G";

/* Altitudes brutes, quatre octets par pixel, et non une image : "image/x-bil;
   bits=32" une fois encodé pour l'URL. */
constexpr const char* WMS_FORMAT_BIL = "image%2Fx-bil%3Bbits%3D32";

/* Le relief d'ensemble de la carte, tel que le jeu le charge : heightmap.png en
   16 bits, étalé de 0 à elev_max. Il bouche les trous du LiDAR, pour que la
   fenêtre fine ne creuse jamais un puits là où le service n'a pas de donnée. */
class ReliefCarte {
public:
    [[nodiscard]] bool charger(const std::filesystem::path& dossierCarte,
                               const CalageCarte&           carte);

    /* Altitude au point le plus proche. Au plus proche suffit : on ne s'en sert
       que dans les trous, et la maille d'ensemble est de toute façon cent fois
       plus grossière que ce qu'on fabrique. */
    [[nodiscard]] float altitude(double lon, double lat) const noexcept;

private:
    std::vector<unsigned short> m_niveaux;
    int                         m_colonnes = 0;
    int                         m_rangees  = 0;
    CalageCarte                 m_carte;
};

/* Écrit l'index à la racine du jeu, dans les mêmes termes que celui des tuiles
   d'image : grille ancrée sur le coin nord-ouest, en coordonnées monde. */
[[nodiscard]] bool ecrireIndexRelief(const std::filesystem::path& sortie,
                                     const std::string&           nomCarte,
                                     const CalageCarte&           carte,
                                     const GrilleRelief&          grille);

/* Découpe un bloc d'altitudes en tuiles et les écrit. Une tuile entièrement
   hors couverture LiDAR n'est pas écrite : le moteur y garde le relief
   d'ensemble. Renvoie le nombre de tuiles écrites, et ajoute leur poids ; -1 si
   une tuile n'a pas pu être écrite, le bloc restant alors à refaire. */
[[nodiscard]] int ecrireBlocRelief(const std::filesystem::path&      sortie,
                                   const std::vector<float>&         altitudes,
                                   const std::vector<unsigned char>& manquant,
                                   int col0, int rangee0, int nCol, int nRangee,
                                   float pasX, float pasZ, std::uintmax_t& octetsEcrits);

/* Marque d'un bloc déjà traité : un fichier sous .blocs, comme les scripts. */
[[nodiscard]] std::filesystem::path cheminMarqueBloc(const std::filesystem::path& sortie,
                                                     int col0, int rangee0);

#ifdef ARTOUSTE_HAS_CURL

/* Grille d'altitudes aux NOEUDS d'un bloc, en mètres, rangée 0 au nord. Les
   bornes sont celles des noeuds extrêmes. Le service est interrogé à
   RELIEF_SUR_ECH fois la finesse demandée et les pixels sont moyennés ici :
   il rééchantillonne au plus proche depuis sa propre pyramide, et un point
   isolé peut tomber sur une arête. */
[[nodiscard]] bool demanderAltitudes(CURL* curl, double lonMin, double lonMax, double latMin,
                                     double latMax, int nx, int nz,
                                     std::vector<float>& sortie, std::uintmax_t& octetsRecus);

/* Un bloc de bout en bout : altitudes demandées au service, trous et points
   aberrants bouchés avec le relief d'ensemble, tuiles écrites et bloc marqué
   fait. Renvoie le nombre de tuiles écrites, ou -1 si le service n'a rien rendu
   de bon après ses essais. */
[[nodiscard]] int traiterBlocRelief(CURL* curl, const CalageCarte& carte,
                                    const GrilleRelief& grille, const ReliefCarte& ensemble,
                                    const std::filesystem::path& sortie, int col0, int rangee0,
                                    int nCol, int nRangee, const std::atomic<bool>& arret,
                                    std::uintmax_t& octetsRecus, std::uintmax_t& octetsEcrits);

#endif /* ARTOUSTE_HAS_CURL */

} /* namespace artouste::app::cartes */
