/*
 * ApplicationMenuCartesInventaire.cpp
 * Ce que chaque carte occupe sur le disque, mesuré à l'ouverture du
 * gestionnaire de cartes.
 *
 * Peser un jeu de tuiles demande de parcourir des dizaines de milliers de
 * fichiers : quelques secondes par carte, pendant lesquelles on rend la main à
 * l'écran d'attente.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "app/cartes/FabriqueTuiles.hpp"
#include "app/menu/cartes/EcranCartesMesures.hpp"
#include "render/tuiles/Pyramide.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>

namespace artouste::app {

/* La variable locale de l'écran s'appelle aussi cartes : on nomme donc le
   sous-système de fabrication autrement. */
namespace fab = artouste::app::cartes;

using ecran_cartes::compterTuiles;
using ecran_cartes::tailleDossier;
using ecran_cartes::tailleFichier;

std::vector<Application::EtatCarte>
Application::inventorierCartes(const std::filesystem::path& assets) {
    const std::vector<MapEntry> cartes = recenserCartes(assets);

    std::vector<EtatCarte> etats;
    for (const MapEntry& carte : cartes) {
        const std::filesystem::path dossier = assets / "terrain" / carte.dir;

        const std::string attente = "Analyse en cours : " + carte.title;
        renderLoadingScreen(attente.c_str(),
                            static_cast<float>(etats.size()) /
                                static_cast<float>(cartes.size()));

        EtatCarte etat;
        etat.dir              = carte.dir;
        etat.titre            = carte.title;
        etat.dossier          = dossier;
        etat.octetsBatiments  = tailleFichier(dossier / "buildings.bin");
        etat.dossierTuiles    = render::tuiles::cheminJeuDeTuiles(dossier, racineTuiles());
        etat.octetsTuiles     = tailleDossier(etat.dossierTuiles);
        /* Finesse du jeu en place : son niveau le plus fin. Ne lit que les
           index, pas les tuiles. */
        if (!etat.dossierTuiles.empty()) {
            const auto niveaux = render::tuiles::ouvrirNiveaux(etat.dossierTuiles);
            if (!niveaux.empty()) {
                etat.finesseTuiles = niveaux.back().calage().mParPixel;
            }
            /* Le témoin est cherché sur CHAQUE niveau : un niveau serré
               interrompu laisserait sinon la carte annoncée entière. */
            for (const render::tuiles::Pyramide& niveau : niveaux) {
                if (fab::fabricationInachevee(niveau.dossier())) {
                    etat.tuilesInachevees = true;
                    etat.tuilesAttendues =
                        niveau.calage().colonnes * niveau.calage().rangees;
                    etat.tuilesPresentes = compterTuiles(niveau.dossier());
                    break;
                }
            }
        }
        etat.interet = fab::interet(dossier);
        /* Le socle : tout le dossier moins les tuiles quand elles y sont
           rangées. Les bâtiments y sont compris, l'écran ne sait pas les
           supprimer à part. */
        const std::uintmax_t brut = tailleDossier(dossier);
        const std::uintmax_t tuilesDedans =
            (etat.dossierTuiles.empty() || etat.dossierTuiles.parent_path() != dossier)
                ? 0
                : etat.octetsTuiles;
        etat.octetsSocle = brut - std::min(brut, tuilesDedans);

        /* On lit m_config et non m_treesEnabled : ce dernier n'est calculé qu'au
           chargement de la scène, alors que cet écran s'ouvre avant. */
        etat.arbres    = m_config.trees && std::getenv("ARTOUSTE_NO_TREES") == nullptr;
        etat.batiments = true;
        std::ifstream options(dossier / "options.txt");
        std::string   cle, valeur;
        while (options >> cle) {
            if (!cle.empty() && cle[0] == '#') {
                std::getline(options, cle);
                continue;
            }
            if (!(options >> valeur)) {
                break;
            }
            const bool oui = !(valeur == "0" || valeur == "non" || valeur == "false");
            if (cle == "arbres") {
                etat.arbres       = oui;
                etat.arbresDefini = true;
            } else if (cle == "batiments") {
                etat.batiments       = oui;
                etat.batimentsDefini = true;
            } else if (cle == "tuiles") {
                etat.tuiles        = oui;
                etat.tuilesDefinie = true;
            }
        }
        etats.push_back(std::move(etat));
    }
    return etats;
}

} /* namespace artouste::app */
