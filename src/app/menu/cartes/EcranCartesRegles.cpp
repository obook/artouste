/*
 * EcranCartesRegles.cpp
 * Règles du gestionnaire de cartes (voir EcranCartesRegles.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/menu/cartes/EcranCartesRegles.hpp"

#include "app/menu/cartes/EcranCartes.hpp"

#include "render/tuiles/Pyramide.hpp"

#include <cstdio>
#include <fstream>
#include <system_error>

namespace artouste::app::ecran_cartes {

namespace {

/* Efface un dossier et dit si c'est fait. Disque occupé, droits insuffisants :
   les fichiers sont toujours là, et vider les compteurs ferait afficher 0 octet
   sur une carte encore pleine, jusqu'au prochain inventaire. */
[[nodiscard]] bool effacerDossier(const std::filesystem::path& dossier) {
    std::error_code effacement;
    std::filesystem::remove_all(dossier, effacement);
    if (effacement) {
        std::fprintf(stderr, "[cartes] suppression de %s impossible : %s\n",
                     dossier.string().c_str(), effacement.message().c_str());
        return false;
    }
    return true;
}

} /* namespace */

bool tuilesEfficaces(const cartes::EtatCarte& carte) {
    return carte.octetsTuiles > 0 &&
           render::tuiles::niveauUtile(carte.finesseTuiles, carte.interet.ortho);
}

bool tuilesDepassees(const cartes::EtatCarte& carte) {
    return carte.octetsTuiles > 0 && !carte.tuilesInachevees && carte.finesseTuiles > 0.0f &&
           carte.interet.visee > 0.0f && carte.finesseTuiles > carte.interet.visee * 1.2f;
}

float finesseAFabriquer(const cartes::EtatCarte& carte) {
    if (carte.tuilesInachevees && carte.finesseTuiles > 0.0f) {
        return carte.finesseTuiles;
    }
    return carte.interet.visee;
}

std::filesystem::path destinationTuiles(const cartes::EtatCarte&     carte,
                                        const std::filesystem::path& racineTuiles) {
    if (!carte.dossierTuiles.empty()) {
        return carte.dossierTuiles;
    }
    if (racineTuiles.empty()) {
        return carte.dossier / "tuiles";
    }
    return racineTuiles / carte.dir;
}

std::filesystem::path destinationRelief(const cartes::EtatCarte&     carte,
                                        const std::filesystem::path& racineTuiles) {
    if (!carte.dossierRelief.empty()) {
        return carte.dossierRelief;
    }
    if (racineTuiles.empty()) {
        return carte.dossier / "relief";
    }
    return racineTuiles / (carte.dir + ".relief");
}

void ecrireOptions(const cartes::EtatCarte& carte) {
    if (!carte.arbresDefini && !carte.batimentsDefini && !carte.tuilesDefinie) {
        std::error_code ec;
        std::filesystem::remove(carte.dossier / "options.txt", ec);
        return;
    }
    std::ofstream out(carte.dossier / "options.txt", std::ios::trunc);
    if (!out) {
        return;
    }
    out << "# Options de la carte, écrites par le gestionnaire de cartes.\n";
    out << "# Le moteur les relit au chargement ; une clé absente rend la main à\n";
    out << "# la configuration générale (assets/config.txt).\n";
    if (carte.arbresDefini) {
        out << "arbres " << (carte.arbres ? 1 : 0) << "\n";
    }
    if (carte.batimentsDefini) {
        out << "batiments " << (carte.batiments ? 1 : 0) << "\n";
    }
    if (carte.tuilesDefinie) {
        out << "tuiles " << (carte.tuiles ? 1 : 0) << "\n";
    }
}

void rendreAuDefaut(cartes::EtatCarte& carte, bool arbresGeneral) {
    std::error_code ec;
    std::filesystem::remove(carte.dossier / "options.txt", ec);
    carte.arbres          = arbresGeneral;
    carte.arbresDefini    = false;
    carte.batiments       = true;
    carte.batimentsDefini = false;
    carte.tuiles          = true;
    carte.tuilesDefinie   = false;
}

void lancerFabrication(Etat& etat) {
    /* La fabrique efface elle-même un jeu d'une autre finesse ou d'un autre pas
       (FabriqueTuiles.cpp, FabriqueReliefBoucle.cpp). */
    cartes::EtatCarte& carte = etat.courante();
    if (etat.surRelief) {
        etat.fabrique.lancerRelief(carte.dossier, destinationRelief(carte, etat.racineTuiles));
    } else {
        etat.fabrique.lancer(carte.dossier,
                             destinationTuiles(carte, etat.racineTuiles),
                             finesseAFabriquer(carte));
    }
    etat.disqueRemanie = true;
}

void supprimerTuiles(Etat& etat) {
    cartes::EtatCarte& carte = etat.courante();
    if (etat.surRelief) {
        if (!effacerDossier(carte.dossierRelief)) {
            return;
        }
        carte.octetsRelief   = 0;
        carte.reliefPresent  = 0;
        carte.reliefInacheve = false;
        carte.dossierRelief.clear();
        etat.disqueRemanie = true;
        return;
    }
    if (!effacerDossier(carte.dossierTuiles)) {
        return;
    }
    carte.octetsTuiles     = 0;
    carte.tuilesPresentes  = 0;
    carte.tuilesInachevees = false;
    carte.finesseTuiles    = 0.0f;
    carte.dossierTuiles.clear();
    etat.disqueRemanie = true;
}

} /* namespace artouste::app::ecran_cartes */
