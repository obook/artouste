/*
 * main.cpp
 * Point d'entrée du simulateur. Lit les options de lancement, crée
 * l'application puis lance sa boucle ; toute la logique vit dans
 * artouste::app::Application, main se contente de la démarrer et de renvoyer
 * son code de sortie.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "app/LigneCommande.hpp"

#include <cstdlib>

int main(int argc, char** argv) {
    const artouste::app::OptionsLancement options =
        artouste::app::lireLigneCommande(argc, argv);
    if (options.aide) {
        artouste::app::afficherAide(argv[0]);
        return EXIT_SUCCESS;
    }
    if (options.erreur) {
        artouste::app::afficherAide(argv[0]);
        return EXIT_FAILURE;
    }

    artouste::app::Application app;
    app.appliquerOptions(options);
    return app.run();
}
