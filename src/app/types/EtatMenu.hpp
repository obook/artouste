/*
 * EtatMenu.hpp
 * Ce que le menu de démarrage a choisi, lu ensuite par la scène.
 *
 * Ces choix l'emportent sur la configuration, mais pas sur les variables
 * d'environnement.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <string>

namespace artouste::app {

struct EtatMenu {
    std::string terrain;      /* vide = pas de choix au menu */
    int         turbine = -1; /* -1 = pas de choix, 0 = à froid, 1 = démarrée */
    bool        demo    = false;
    bool        combat  = false; /* mode zombie */
};

} /* namespace artouste::app */
