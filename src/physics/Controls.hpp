/*
 * Controls.hpp
 * Commandes du pilote, normalisées entre -1 et +1.
 * Sert de contrat simple entre la couche d'entrée (clavier, manette) et la physique.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::physics {

struct Controls {
    float cyclicLateral      = 0.0f;  /* [-1, +1]  manche latéral      -> roulis */
    float cyclicLongitudinal = 0.0f;  /* [-1, +1]  manche longitudinal -> tangage */
    float collective         = 0.0f;  /* [ 0, +1]  levier collectif    -> montée/descente */
    float pedals             = 0.0f;  /* [-1, +1]  palonniers          -> lacet */
};

}  /* namespace artouste::physics */
