/*
 * main.cpp
 * Point d'entrée du simulateur. Crée l'application puis lance sa boucle ;
 * toute la logique vit dans artouste::app::Application, main se contente
 * de la démarrer et de renvoyer son code de sortie.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "app/Application.hpp"

int main() {
    artouste::app::Application app;
    return app.run();
}
