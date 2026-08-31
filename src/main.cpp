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
#include "app/CartesGraphiques.hpp"
#include "app/LigneCommande.hpp"

#include <cstdlib>

#ifdef _WIN32
/*
Les pilotes NVIDIA et AMD lisent ces deux symboles au démarrage du processus et
confient alors le rendu à la carte dédiée plutôt qu'à la puce intégrée, ce qui
évite au joueur de plafonner à la moitié de la fréquence de son écran. Le
mécanisme est propre à Windows : sous Linux, la sélection passe par des
variables d'environnement, posées par play-linux.sh.
*/
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement                  = 1;
__declspec(dllexport) int           AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char** argv) {
    const artouste::app::OptionsLancement options =
        artouste::app::lireLigneCommande(argc, argv);
    if (options.aide) {
        artouste::app::afficherAide(argv[0]);
        return EXIT_SUCCESS;
    }
    if (options.gpu) {
        artouste::app::afficherCartesGraphiques();
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
