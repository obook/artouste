/*
 * CycleJourNuit.cpp
 * Calcul de l'heure simulée (voir CycleJourNuit.hpp).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/CycleJourNuit.hpp"

#include <algorithm>
#include <cmath>

namespace artouste::app {

namespace {

constexpr float JOUR = 86400.0f;           /* secondes dans une journée */
constexpr float LEVER = 6.0f * 3600.0f;    /* le soleil passe l'horizon */
constexpr float COUCHER = 18.0f * 3600.0f; /* et repasse dessous */
constexpr float MOITIE = JOUR * 0.5f;      /* 12 h de jour, 12 h de nuit */

/* Ramène une heure quelconque dans [0, 86400[. */
float normaliser(float secondes) {
    const float s = std::fmod(secondes, JOUR);
    return (s < 0.0f) ? s + JOUR : s;
}

} /* namespace */

float heureDuJour(float baseSecondes, float t, float vitesseJour, float facteurNuit) {
    /* Temps figé, ou marche arrière : rien à découper en deux vitesses, la
       formule uniforme suffit et reste réversible. */
    if (vitesseJour <= 0.0f) {
        return normaliser(baseSecondes + t * vitesseJour);
    }

    /* Un facteur nul ou négatif arrêterait la nuit pour toujours : on le borne
       ici aussi, la configuration n'étant pas le seul appelant possible. */
    const float vitesseNuit = vitesseJour * std::max(facteurNuit, 0.1f);
    const float dureeJour = MOITIE / vitesseJour; /* durée réelle du jour simulé */
    const float dureeNuit = MOITIE / vitesseNuit;
    const float dureeCycle = dureeJour + dureeNuit;

    /* Où l'heure de départ nous place-t-elle dans le cycle, en temps réel écoulé
       depuis le dernier lever ? */
    const float base = normaliser(baseSecondes);
    float phase = 0.0f;
    if (base >= LEVER && base < COUCHER) {
        phase = (base - LEVER) / vitesseJour;
    } else {
        const float depuisCoucher = (base >= COUCHER) ? (base - COUCHER) : (base + JOUR - COUCHER);
        phase = dureeJour + depuisCoucher / vitesseNuit;
    }

    float avance = std::fmod(phase + t, dureeCycle);
    if (avance < 0.0f) {
        avance += dureeCycle; /* dates antérieures au lancement */
    }
    if (avance < dureeJour) {
        return LEVER + avance * vitesseJour;
    }
    return normaliser(COUCHER + (avance - dureeJour) * vitesseNuit);
}

} /* namespace artouste::app */
