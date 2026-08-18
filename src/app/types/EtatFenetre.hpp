/*
 * EtatFenetre.hpp
 * Plein écran, et position de la fenêtre à restaurer en sortant.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::app {

struct EtatFenetre {
    bool pleinEcran = false;
    int  x = 0;
    int  y = 0;
    int  largeur = 0;
    int  hauteur = 0;
};

} /* namespace artouste::app */
