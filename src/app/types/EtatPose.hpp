/*
 * EtatPose.hpp
 * Aide au posé : score du dernier atterrissage et anti-rebond.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::app {

struct EtatPose {
    float scoreS   = 0.0f;  /* temps restant d'affichage du score (s) */
    float derniereDistanceM = -1.0f; /* distance au posé, -1 si aucun */

    bool auSolAvant = false; /* état sol de l'image précédente (anti-rebond) */
    bool aVole      = false; /* a volé depuis l'activation : évite un faux score au sol */
    bool aDecolle   = false; /* a décollé depuis le lancement : pas d'aide avant */

    float graceReticuleS = 0.0f; /* s sans réticule après un décollage du pad */
};

} /* namespace artouste::app */
