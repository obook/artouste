/*
 * EtatRadio.hpp
 * Message radio en cours : compte à rebours, texte, durée d'affichage.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <string>

namespace artouste::app {

struct EtatRadio {
    bool  arme  = false; /* compte à rebours en cours */
    bool  emis  = false; /* déjà émis depuis le dernier démarrage turbine */
    float delaiS = 0.0f; /* s avant émission, une fois armé */
    float afficheS = 0.0f; /* s restantes d'affichage du sous-titre */

    std::string message;       /* texte courant (anglais) */
    std::string stationDepart; /* hélipad de départ, sert de tour de contrôle */
};

} /* namespace artouste::app */
