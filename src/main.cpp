// artouste - point d'entrée.
// Jalon M1 : hélicoptère statique (placeholder procédural), terrain en damier,
// rotors animés à régime fixe, caméra en orbite. Toute la logique est dans
// artouste::app::Application ; main se contente de la lancer.

#include "app/Application.hpp"

int main() {
    artouste::app::Application app;
    return app.run();
}
