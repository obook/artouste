/*
 * ApplicationRun.cpp
 * Déroulé d'une session : menu, chargement de la scène, vol, retour au menu.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "render/Terrain.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace artouste::app {

int Application::run() {
    /* Ressources présentes ? Vérifié avant d'ouvrir la moindre fenêtre : si "assets"
       manque (exe lancé depuis le zip non extrait), on prévient l'utilisateur par un
       message natif et on quitte, au lieu de planter en silence. */
    if (!assetsDisponibles()) {
        return EXIT_FAILURE;
    }
    /* Configuration lue avant l'ouverture de la fenêtre : le MSAA doit être connu à
       la création du contexte GLFW. Réutilisée ensuite par initScene. */
    m_config = loadConfig(resolveAssetDir() / "config.txt");
    /* Recherche d'une version plus récente, lancée tout de suite et menée dans un
       fil séparé : elle a ainsi le temps d'aboutir pendant l'ouverture de la
       fenêtre, sans jamais la retarder. Le menu de démarrage lira le résultat et
       proposera, le cas échéant, d'ouvrir la page du projet. */
    if (m_config.checkUpdate && std::getenv("ARTOUSTE_NO_MAJ") == nullptr) {
        m_maj.lancer();
    }
    if (!initWindow()) {
        return EXIT_FAILURE;
    }
    if (!initGL()) {
        return EXIT_FAILURE;
    }

    /* Curseur masqué dès l'ouverture de la fenêtre. Seuls les menus le rétablissent,
       le temps qu'ils sont affichés (voir runStartupMenu et runGestionnaireCartes) :
       ailleurs il n'a rien à faire là, ni en vol ni sur un écran de chargement. Le
       masquer seulement à l'entrée en vol le laissait traîner sur le chargement de la
       carte, d'autant plus visible que celui-ci s'est allongé. */
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    /* Plein écran sans bordure au lancement, sauf en mode capture (qui garde son
       1280x720 pour un cadrage reproductible) ou si ARTOUSTE_WINDOWED force le fenêtré
       (pratique en développement). La touche F bascule ensuite à volonté. */
    if (std::getenv("ARTOUSTE_SCREENSHOT") == nullptr &&
        std::getenv("ARTOUSTE_WINDOWED") == nullptr) {
        setFullscreen(true);
    }

    /* Menu de démarrage dans la fenêtre (remplace launch.bat, bloqué par le Contrôle
       intelligent des applications de Windows). Sauté en mode capture, quand la carte
       est déjà imposée par une variable d'environnement (scripts, tests) ou par une
       option de ligne de commande (--carte, --monument...), ou sur demande explicite
       (ARTOUSTE_NO_MENU). */
    /* ARTOUSTE_CARTES : ouvre directement le gestionnaire de cartes, sans passer
       par le menu. Sert à en faire le tour sans lancer de vol, et à le tester
       d'une seule commande. */
    if (std::getenv("ARTOUSTE_CARTES") != nullptr) {
        m_hud.init(m_window);
        runGestionnaireCartes();
        return EXIT_SUCCESS;
    }

    const bool menuDemande = std::getenv("ARTOUSTE_SCREENSHOT") == nullptr &&
                             std::getenv("ARTOUSTE_TERRAIN") == nullptr &&
                             std::getenv("ARTOUSTE_NO_MENU") == nullptr &&
                             !m_options.sauteMenu();
    if (menuDemande) {
        m_hud.init(m_window);  /* initialise ImGui (idempotent) avant d'afficher le menu */
        if (!runStartupMenu()) {
            return EXIT_SUCCESS;  /* l'utilisateur a fermé la fenêtre sans lancer */
        }
        /* Chargement de la scène (terrain, hélicoptère...) : bloquant et sans retour
           visuel propre (voir initScene/loadTerrain). Ce message reste affiché tant
           qu'une étape ne le remplace pas par un message plus précis. */
        renderLoadingScreen("Chargement...");
    }

    try {
        initScene();
        /*
         * Mode capture : si ARTOUSTE_SCREENSHOT indique un chemin, on rend une
         * image, on l'enregistre, puis on quitte (pratique pour vérifier le
         * rendu sans lancer la boucle interactive).
         */
        if (const char* shot = std::getenv("ARTOUSTE_SCREENSHOT")) {
            captureScreenshot(shot);
            return EXIT_SUCCESS;
        }
        /* Boucle menu <-> vol : la touche Échap en vol rend la main pour réafficher le
           menu (choix d'une autre carte, turbine à froid ou non). Échap ou "Quitter"
           dans le menu -- comme la fermeture de la fenêtre -- termine l'application. Le
           curseur souris, lui, est masqué partout sauf dans les menus, qui le
           rétablissent eux-mêmes le temps de leur affichage (voir plus haut). */
        for (;;) {
            const bool retourMenu = mainLoop();
            if (!retourMenu) {
                break;  /* fenêtre fermée : on quitte */
            }
            if (!menuDemande || !runStartupMenu()) {
                break;  /* pas de menu (carte imposée), ou Échap/Quitter dans le menu */
            }
            renderLoadingScreen("Chargement...");
            applyMenuSession();  /* applique le nouveau choix, puis on repart en vol */
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur fatale : %s\n", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} /* namespace artouste::app */
