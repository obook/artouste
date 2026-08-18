/*
 * ApplicationCaptureReglages.hpp
 * Lecture des variables ARTOUSTE_SHOT_*, partagée par le cadrage et le HUD de
 * capture.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <cstdlib>

namespace artouste::app {

/* Réglage numérique de capture, lu dans une variable d'environnement. Une
   variable absente laisse la valeur par défaut : c'est ce qui rend une capture
   reproductible sans avoir à toutes les fixer. */
[[nodiscard]] inline float shotFloat(const char* nom, float defaut) {
    const char* valeur = std::getenv(nom);
    if (valeur == nullptr) {
        return defaut;
    }
    return std::strtof(valeur, nullptr);
}

/* Réglage tout ou rien : seule la présence de la variable compte, sa valeur
   n'est pas lue. */
[[nodiscard]] inline bool shotFlag(const char* nom) {
    return std::getenv(nom) != nullptr;
}

} /* namespace artouste::app */
