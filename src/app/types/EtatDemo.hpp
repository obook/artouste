/*
 * EtatDemo.hpp
 * Ce que la démonstration automatique retient d'une image à l'autre.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::app {

struct EtatDemo {
    bool etaitActive = false; /* pour couper la musique quand la démo s'arrête */
    bool vueReprise  = false; /* l'utilisateur a repris la main sur la vue (touche C) */
    bool hudRepris   = false; /* l'utilisateur a repris la main sur le HUD (touche H) */

    /* Démo lancée depuis le menu : en sortir ramène au menu, au lieu de rendre
       la main en vol libre. Celle lancée par ARTOUSTE_DEMO garde l'ancien
       comportement. */
    bool depuisMenu = false;

    /* Délai de grâce sur les commandes après le lancement depuis le menu. La
       touche D lance la démo et sert aussi de palonnier droit en vol : sans ce
       délai, le résidu de la frappe coupait la démo aussitôt. */
    float graceS = 0.0f;
};

} /* namespace artouste::app */
