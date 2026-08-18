/*
 * ApplicationMenuEntrees.hpp
 * Lecture des entrées du menu de démarrage : clavier et première manette.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

struct GLFWwindow;

namespace artouste::app {

/* Touches et boutons du menu, à l'état brut (les fronts sont calculés par
   l'appelant). */
struct EntreesMenu {
    bool haut    = false;
    bool bas     = false;
    bool valider = false;
    bool turbine = false;
    bool demo    = false;
    bool quitter = false;
};

[[nodiscard]] EntreesMenu lireEntreesMenu(GLFWwindow* fenetre);

} /* namespace artouste::app */
