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

#include "app/cartes/FabriqueRelief.hpp"
#include "app/cartes/FabriqueTuiles.hpp"
#include "app/menu/cartes/EcranCartesMesures.hpp"
#include "render/relief/FenetreRelief.hpp"
#include "render/tuiles/Pyramide.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

namespace artouste::app {

/* La variable locale de l'écran s'appelle aussi cartes : on nomme donc le
   sous-système de fabrication autrement. */
namespace fab = artouste::app::cartes;

using ecran_cartes::caractereTournant;
using ecran_cartes::compterTuiles;
using ecran_cartes::tailleDossier;
using ecran_cartes::tailleFichier;

/* Dernier redessin de l'écran d'attente, partagé par les cartes d'un même
   inventaire. Le rouet doit tourner PENDANT les mesures, et non entre elles :
   peser un jeu de tuiles prend des secondes, et l'écran resterait figé tout ce
   temps. Dix images par seconde suffisent à l'oeil, et chacune attend la
   synchronisation verticale : en dessiner davantage ralentirait la mesure sans
   rien apporter. */
namespace {
double dernierDessin = 0.0;
} /* namespace */

Application::EtatCarte Application::inventorierCarte(const std::filesystem::path& assets,
                                                    const MapEntry& carte, float part) {
    const std::filesystem::path dossier = assets / "terrain" / carte.dir;

    const std::string attente = "Analyse en cours : " + carte.title;
    const auto dessiner = [&] {
        dernierDessin = glfwGetTime();
        const std::string ligne =
            std::string(1, caractereTournant(dernierDessin)) + " " + attente;
        renderLoadingScreen(ligne.c_str(), part);
    };
    /* Passé aux marches de dossier, qui l'appellent de loin en loin. */
    const ecran_cartes::Battement battre = [&] {
        if (glfwGetTime() - dernierDessin >= 0.1) {
            dessiner();
        }
    };
    dessiner();

    EtatCarte etat;
    etat.dir              = carte.dir;
    etat.titre            = carte.title;
    etat.dossier          = dossier;
    etat.octetsBatiments  = tailleFichier(dossier / "buildings.bin");
    etat.dossierTuiles    = render::tuiles::cheminJeuDeTuiles(dossier, racineTuiles());
    etat.octetsTuiles     = tailleDossier(etat.dossierTuiles, battre);
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
                etat.tuilesPresentes = compterTuiles(niveau.dossier(), ".dds", battre);
                break;
            }
        }
    }
    /* Le relief fin vit à côté des tuiles d'image, et le moteur le cherche
       aux mêmes endroits : on lui demande où il le trouverait. */
    etat.dossierRelief = render::relief::cheminJeuDeRelief(dossier, racineTuiles());
    etat.octetsRelief  = tailleDossier(etat.dossierRelief, battre);
    if (!etat.dossierRelief.empty()) {
        const fab::GrilleRelief pose  = fab::lireIndexRelief(etat.dossierRelief);
        const fab::GrilleRelief voulu = fab::grilleRelief(dossier);
        etat.reliefPasX = pose.pasX;
        etat.reliefPasZ = pose.pasZ;
        /* Même tolérance que la fabrique, qui efface un jeu d'un autre pas
           avant de refaire (FabriqueReliefBoucle.cpp). */
        etat.reliefAutrePas = pose.valide && voulu.valide &&
                              (std::fabs(pose.pasX - voulu.pasX) > 1e-3f ||
                               std::fabs(pose.pasZ - voulu.pasZ) > 1e-3f);
        if (fab::fabricationInachevee(etat.dossierRelief)) {
            etat.reliefInacheve = true;
            etat.reliefAttendu  = pose.tuiles();
            etat.reliefPresent  = compterTuiles(etat.dossierRelief, ".r16", battre);
        }
    }
    etat.interet = fab::interet(dossier);
    /* Le socle : tout le dossier moins les tuiles quand elles y sont
       rangées. Les bâtiments y sont compris, l'écran ne sait pas les
       supprimer à part. */
    const std::uintmax_t brut = tailleDossier(dossier, battre);
    const std::uintmax_t tuilesDedans =
        (etat.dossierTuiles.empty() || etat.dossierTuiles.parent_path() != dossier)
            ? 0
            : etat.octetsTuiles;
    /* Le relief se range souvent DANS la carte (<carte>/relief) : sans cette
       soustraction, ses centaines de mégaoctets gonfleraient le socle. */
    const std::uintmax_t reliefDedans =
        (etat.dossierRelief.empty() || etat.dossierRelief.parent_path() != dossier)
            ? 0
            : etat.octetsRelief;
    etat.octetsSocle = brut - std::min(brut, tuilesDedans + reliefDedans);

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
    return etat;
}

std::vector<Application::EtatCarte>
Application::inventorierCartes(const std::filesystem::path& assets) {
    const std::vector<MapEntry> cartes = recenserCartes(assets);

    std::vector<EtatCarte> etats;
    for (const MapEntry& carte : cartes) {
        etats.push_back(inventorierCarte(
            assets, carte,
            static_cast<float>(etats.size()) / static_cast<float>(cartes.size())));
    }
    return etats;
}

} /* namespace artouste::app */
