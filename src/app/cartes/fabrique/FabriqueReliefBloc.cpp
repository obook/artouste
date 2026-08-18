/*
 * FabriqueReliefBloc.cpp
 * Un bloc de relief : demande des altitudes, bouchage des trous, écriture.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/cartes/fabrique/FabriqueReliefInterne.hpp"

#include <chrono>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace artouste::app::cartes {

#ifdef ARTOUSTE_HAS_CURL

namespace {

/* Marque de bloc traité, comme celle des scripts (tools/terrain/tuiles_grille.py).
   On ne se fie pas à la présence des tuiles : un bloc entièrement hors couverture
   LiDAR n'en écrit aucune, et serait retéléchargé indéfiniment. */
void marquerBloc(const std::filesystem::path& sortie, int col0, int rangee0) {
    std::error_code ec;
    std::filesystem::create_directories(sortie / ".blocs", ec);
    std::ofstream out(cheminMarqueBloc(sortie, col0, rangee0), std::ios::trunc);
    out << "fait\n";
}

} /* namespace */

int traiterBlocRelief(CURL* curl, const CalageCarte& carte, const GrilleRelief& grille,
                      const ReliefCarte& ensemble, const std::filesystem::path& sortie, int col0,
                      int rangee0, int nCol, int nRangee, const std::atomic<bool>& arret,
                      std::uintmax_t& octetsRecus, std::uintmax_t& octetsEcrits) {
    const double degParMLon = (carte.lonMax - carte.lonMin) / static_cast<double>(carte.largeurM);
    const double degParMLat = (carte.latMax - carte.latMin) / static_cast<double>(carte.hauteurM);
    const double tuileX = static_cast<double>(RELIEF_TUILE_POINTS * grille.pasX);
    const double tuileZ = static_cast<double>(RELIEF_TUILE_POINTS * grille.pasZ);

    /* Emprise du bloc. La grille est ancrée sur le coin nord-ouest, et la rangée
       0 est au nord : la latitude décroît quand la rangée croît. */
    const double lonLo = carte.lonMin + static_cast<double>(col0) * tuileX * degParMLon;
    const double lonHi = lonLo + static_cast<double>(nCol) * tuileX * degParMLon;
    const double latHi = carte.latMax - static_cast<double>(rangee0) * tuileZ * degParMLat;
    const double latLo = latHi - static_cast<double>(nRangee) * tuileZ * degParMLat;

    const int    nx     = nCol * RELIEF_TUILE_POINTS;
    const int    nz     = nRangee * RELIEF_TUILE_POINTS;
    const double pasLon = (lonHi - lonLo) / static_cast<double>(nx);
    const double pasLat = (latHi - latLo) / static_cast<double>(nz);

    /* Noeuds EXTRÊMES du bloc : le premier tombe sur le coin nord-ouest de la
       tuile, le dernier un pas AVANT le coin suivant, les tuiles ne se
       recouvrant pas. */
    std::vector<float> altitudes;
    bool               recu = false;
    for (int essai = 0; essai < 3 && !recu && !arret.load(); ++essai) {
        recu = demanderAltitudes(curl, lonLo, lonHi - pasLon, latLo + pasLat, latHi, nx, nz,
                                 altitudes, octetsRecus);
        if (!recu) {
            std::this_thread::sleep_for(std::chrono::seconds(2 * (essai + 1)));
        }
    }
    if (!recu) {
        return arret.load() ? 0 : -1;
    }

    /* Trous et points aberrants bouchés avec le relief d'ensemble : la fenêtre
       fine ne doit jamais creuser un puits là où le service n'a pas de donnée,
       ni suivre un point du laser passé sous le sol. */
    std::vector<unsigned char> manquant(altitudes.size(), 0);
    for (int j = 0; j < nz; ++j) {
        const double lat = latHi - static_cast<double>(j) * pasLat;
        for (int i = 0; i < nx; ++i) {
            const std::size_t k =
                static_cast<std::size_t>(j) * static_cast<std::size_t>(nx) +
                static_cast<std::size_t>(i);
            const float sol = ensemble.altitude(lonLo + static_cast<double>(i) * pasLon, lat);
            manquant[k]     = (altitudes[k] <= RELIEF_NODATA) ? 1u : 0u;
            if (manquant[k] != 0u || altitudes[k] < sol - CHUTE_ABERRANTE_M) {
                altitudes[k] = sol;
            }
        }
    }

    const int ecrites = ecrireBlocRelief(sortie, altitudes, manquant, col0, rangee0, nCol,
                                         nRangee, grille.pasX, grille.pasZ, octetsEcrits);
    if (ecrites < 0) {
        return -1; /* bloc non marqué : il sera repris tel quel */
    }
    marquerBloc(sortie, col0, rangee0);
    return ecrites;
}

#endif /* ARTOUSTE_HAS_CURL */

} /* namespace artouste::app::cartes */
