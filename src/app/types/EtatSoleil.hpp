/*
 * EtatSoleil.hpp
 * Où en est la course du soleil pour la carte en cours.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::app {

struct EtatSoleil {
    /* Vitesse du temps : 1 = réel, 144 = jour en 10 min, 0 = figé. */
    float vitesse = 1.0f;

    /* Multiplicateur appliqué entre le coucher et le lever (clé lune_vitesse) :
       la nuit passe deux fois plus vite que le jour par défaut. */
    float vitesseNuit = 2.0f;

    /* Heure de départ, en secondes depuis minuit. */
    float heureDepart = 0.0f;

    /* Valeur de m_animTime quand l'heure de départ a été fixée. Le temps
       d'animation court depuis le lancement et ne repart jamais de zéro : sans
       cette origine, la deuxième carte d'une session reprenait son heure de
       départ AUGMENTÉE de tout le temps déjà joué. */
    float origine = 0.0f;
};

} /* namespace artouste::app */
