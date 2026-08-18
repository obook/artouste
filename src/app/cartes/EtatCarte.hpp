/*
 * EtatCarte.hpp
 * Ce qu'une carte occupe sur le disque et ce qu'elle affiche.
 *
 * Remplie par Application::inventorierCartes, lue par le gestionnaire de
 * cartes (app/menu/cartes/).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/cartes/FabriqueTuiles.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace artouste::app::cartes {

struct EtatCarte {
    std::string           dir;
    std::string           titre;
    std::filesystem::path dossier;
    std::filesystem::path dossierTuiles; /* vide si la carte n'a pas de tuiles */
    std::uintmax_t        octetsSocle     = 0;
    std::uintmax_t        octetsBatiments = 0;
    std::uintmax_t        octetsTuiles    = 0;

    /* Finesse du jeu en place (m/px, 0 s'il n'y en a pas) et finesse visée. Le
       moteur écarte au chargement un jeu qui n'est pas plus fin que
       l'orthophoto d'ensemble : comparer les deux est la seule façon de savoir
       si ces mégaoctets servent. */
    float   finesseTuiles = 0.0f;
    Interet interet;

    /* Fabrication interrompue : index complet, tuiles partielles. */
    bool tuilesInachevees = false;
    int  tuilesPresentes  = 0;
    int  tuilesAttendues  = 0; /* colonnes x rangées annoncées par l'index */

    /* Options de la carte. Le drapeau "défini" dit si la carte tranche
       elle-même ; sinon la valeur vient de la configuration générale. */
    bool arbres          = true;
    bool arbresDefini    = false;
    bool batiments       = true;
    bool batimentsDefini = false;
    bool tuiles          = true;
    bool tuilesDefinie   = false;
};

} /* namespace artouste::app::cartes */
