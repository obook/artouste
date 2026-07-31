/*
 * ApplicationSceneConfig.cpp
 * Lecture de la configuration et des variables d'environnement ARTOUSTE_*
 * (démo, arbres, terrain, turbine, radio), chargement du terrain choisi,
 * initialisation des systèmes d'entrée/audio/HUD, chargement du modèle 3D de
 * l'appareil et démarrage de la démo ou du combat demandés. Appelée par
 * initScene (ApplicationScene.cpp) après initSceneShaders
 * (ApplicationSceneShaders.cpp).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "app/Config.hpp"
#include "input/InputSystem.hpp"
#include "render/HelicopterModel.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"
#include "util/Math.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace artouste::app {

void Application::initSceneConfig() {
    const std::filesystem::path& assets = m_assetsDir;

    /*
     * Terrain réel (relief IGN + orthophoto drapée). Le terrain à charger est un
     * sous-dossier de assets/terrain/ (par exemple "ossau" ou "cote-landes"),
     * choisi par la clé "terrain" du fichier de configuration. La variable
     * d'environnement ARTOUSTE_TERRAIN, si elle est définie, a la priorité (pratique
     * pour tester une autre map sans toucher au fichier).
     */
    /* Configuration déjà chargée au lancement (run(), avant l'ouverture de la fenêtre
       pour le MSAA). On la réutilise ici plutôt que de relire le fichier. */
    const app::Config& config = m_config;

    /* Mode démo automatique : clé "demo" de la configuration, surchargée par la
       variable d'environnement ARTOUSTE_DEMO (prioritaire). */
    bool demoEnabled = config.demo;
    if (const char* env = std::getenv("ARTOUSTE_DEMO"); env != nullptr && env[0] != '\0') {
        demoEnabled = (env[0] != '0');
    }

    /* Végétation : clé "arbres" de la configuration, forcée à faux par la variable
       d'environnement ARTOUSTE_NO_TREES (prioritaire). Retenue dans un membre car
       c'est loadTerrain (appelé ici puis à chaque changement de carte) qui sème. */
    m_treesEnabled = config.trees;
    if (std::getenv("ARTOUSTE_NO_TREES") != nullptr) {
        m_treesEnabled = false;
    }

    /* Souffle rotor : clé "souffle" de la configuration, forcée à faux par la
       variable d'environnement ARTOUSTE_NO_SOUFFLE (prioritaire). Le nuage n'est
       ni semé ni dessiné quand elle est fausse ; les ressources graphiques, elles,
       ont déjà été créées par initSceneShaders (voir m_souffleFx). */
    m_souffleEnabled = config.rotorWash;
    if (std::getenv("ARTOUSTE_NO_SOUFFLE") != nullptr) {
        m_souffleEnabled = false;
    }

    /* Budget d'arbres : clé "arbres_max" de la config, surchargée par la variable
       d'environnement ARTOUSTE_TREE_MAX (prioritaire). Passé à Vegetation par
       loadTerrain. C'est le principal levier de performance (poste de rendu le plus
       coûteux sur GPU intégré). */
    m_treeBudget = static_cast<std::size_t>(std::max(0, config.treeBudget));
    if (const char* env = std::getenv("ARTOUSTE_TREE_MAX"); env != nullptr && env[0] != '\0') {
        const long v = std::strtol(env, nullptr, 10);
        if (v > 0) {
            m_treeBudget = static_cast<std::size_t>(v);
        }
    }

    /* Fenêtre de tuiles fines : clé "tuiles_fenetre_px", surchargée par
       ARTOUSTE_TUILES_FENETRE. Passée à Terrain par loadTerrain. C'est le second
       levier de mémoire vidéo après les arbres, et le seul qui plafonne le coût
       d'une carte fine quelle que soit son emprise. */
    m_detailWindowPx = std::max(0, config.detailWindowPx);
    if (const char* env = std::getenv("ARTOUSTE_TUILES_FENETRE");
        env != nullptr && env[0] != '\0') {
        const long v = std::strtol(env, nullptr, 10);
        if (v >= 0) {
            m_detailWindowPx = static_cast<int>(v);
        }
    }

    /* Budget de sommets du relief : clé "relief_sommets_max", surchargée par
       ARTOUSTE_RELIEF_SOMMETS. Passé à Terrain par loadTerrain. */
    m_reliefVertexBudget = std::max(0, config.reliefVertexBudget);
    if (const char* env = std::getenv("ARTOUSTE_RELIEF_SOMMETS");
        env != nullptr && env[0] != '\0') {
        m_reliefVertexBudget = static_cast<int>(std::max(0L, std::strtol(env, nullptr, 10)));
    }

    std::string terrainName = config.terrain;
    if (!m_menuTerrain.empty()) { /* choix du menu de démarrage, au-dessus de la config */
        terrainName = m_menuTerrain;
    }
    if (!m_options.carte.empty()) { /* option --carte, au-dessus du menu */
        terrainName = m_options.carte;
    }
    if (const char* env = std::getenv("ARTOUSTE_TERRAIN"); env != nullptr && env[0] != '\0') {
        terrainName = env; /* variable d'environnement : priorité maximale */
    }
    /* La démo se déroule sur le bassin d'Arcachon (survol du cap Ferret puis d'Arcachon). */
    if ((demoEnabled || m_menuDemo) && terrainName != "arcachon") {
        std::printf("[scène] mode démo : terrain forcé sur arcachon.\n");
        terrainName = "arcachon";
    }
    renderLoadingScreen("Chargement du terrain...");
    loadTerrain(terrainName);

    /* Démarrage immédiat (gain de temps en test) : turbine et rotor d'emblée au
       régime, au lieu de la séquence de démarrage (~1 min). Activé par la clé
       `turbine_demarree` de la config, ou la variable d'environnement
       ARTOUSTE_TURBINE_DEMARREE (prioritaire). */
    bool turbineRunning = config.turbineRunning;
    if (m_menuTurbine >= 0) { /* choix du menu de démarrage, au-dessus de la config */
        turbineRunning = (m_menuTurbine == 1);
    }
    /* Point d'apparition demandé sur la ligne de commande : la turbine doit
       tourner. Apparaître à 300 m au-dessus d'un monument avec un rotor arrêté,
       c'est entamer une chute, pas un vol. ARTOUSTE_TURBINE_DEMARREE reste lue
       après, et peut donc encore l'éteindre pour qui le voudrait vraiment. */
    if (m_options.aPointDapparition()) {
        turbineRunning = true;
    }
    if (const char* env = std::getenv("ARTOUSTE_TURBINE_DEMARREE");
        env != nullptr && env[0] != '\0') {
        turbineRunning = (env[0] != '0'); /* variable d'environnement : priorité maximale */
    }
    /* En mode démo, c'est la démo qui pilote la turbine (démarrage rapide) : on
       ignore donc le démarrage immédiat éventuel. */
    if (turbineRunning && !demoEnabled && !m_menuDemo) {
        m_flight.turbine().forceRunning();
        std::printf("[scène] démarrage immédiat : turbine et rotor au régime.\n");
    }

    m_helicopter = std::make_unique<render::HelicopterModel>();
    m_input = std::make_unique<input::InputSystem>(m_window);
    m_hud.init(m_window);
    m_audio.init(assets / "models" / "Alouette-II" / "Sounds");
    /* Mode zombie : dossier des sons ponctuels, séparé des sons de l'hélicoptère
       (voir AudioEngine::initCombatSounds). Fichiers absents pour l'instant :
       silencieux, sans erreur -- à fournir ultérieurement dans
       assets/sounds/combat/ (gunfire.wav, zombie_hit.wav, zombie_death.wav,
       toxic_throw.wav, toxic_impact.wav, wave_start.wav). */
    m_audio.initCombatSounds(assets / "sounds" / "combat");

    /* Flux radio internet : URL de la clé "radio_url" de la config, surchargée par
       la variable d'environnement ARTOUSTE_RADIO_URL (prioritaire). Vide = pas de
       radio. On mémorise l'URL sans démarrer le flux : la radio est coupée au
       lancement, c'est la touche K qui l'allume ou la coupe. */
    m_radioUrl = config.radioUrl;
    if (const char* env = std::getenv("ARTOUSTE_RADIO_URL"); env != nullptr && env[0] != '\0') {
        m_radioUrl = env;
    }

    /*
     * On utilise le vrai modèle FlightGear s'il est présent : le sous-ensemble
     * nécessaire est versionné dans le dépôt (le paquet FlightGear complet, lui,
     * reste local). Sinon on conserve l'hélicoptère dessiné par le code.
     */
    const std::filesystem::path modelsDir = assets / "models" / "Alouette-II" / "Models";
    if (std::filesystem::exists(modelsDir / "alouette.ac")) {
        renderLoadingScreen("Chargement de l'hélicoptère...");
        auto loaded = std::make_unique<render::LoadedHelicopter>(modelsDir);
        if (loaded->loaded()) {
            m_loadedHeli = std::move(loaded);
            /* Livrée par défaut (Gendarmerie) appliquée d'emblée. */
            m_loadedHeli->setLivery(m_livery);
            std::printf("[scène] modèle FlightGear chargé.\n");
        } else {
            std::printf("[scène] échec du chargement du modèle, repli procédural.\n");
        }
    } else {
        std::printf("[scène] modèle absent, repli procédural.\n");
    }

    if (m_height > 0) {
        m_camera.setAspect(static_cast<float>(m_width) / static_cast<float>(m_height));
    }

    /* Mode démo demandé au lancement : on démarre la démonstration tout de suite. Lancée
       depuis le menu (bouton "Démo"), en sortir ramènera au menu (m_demoFromMenu). */
    if (demoEnabled || m_menuDemo) {
        std::printf("[scène] mode démo activé : démonstration automatique en boucle.\n");
        m_demoFromMenu = m_menuDemo;
        startDemo();
    }

    /* Mode zombie demandé au menu (bouton "Mode Zombie", visible seulement sur les
       cartes compatibles, voir ApplicationMenu.cpp) : démarre la session de combat sur
       le terrain qui vient d'être chargé. Sans effet si la carte n'a pas de
       zombies.txt (CombatMode::active() reste faux). */
    /* ARTOUSTE_SHOT_ZOMBIE : arme le mode zombie sans passer par le menu, qui est
       justement sauté en mode capture (voir ApplicationCapture) -- sans quoi
       aucune capture ne pourrait montrer la horde. */
    if (m_menuCombat || std::getenv("ARTOUSTE_SHOT_ZOMBIE") != nullptr) {
        m_combat.start(assets / "terrain" / m_terrainName,
                       [this](float x, float z) { return m_terrain->heightAt(x, z); });
        if (m_combat.active()) {
            /* Vue bloquée en cockpit pendant tout le combat (voir ApplicationInput.cpp,
               qui empêche d'en sortir) : c'est la vue la plus immersive pour viser au
               canon fixe, et elle évite les vues externes qui ne serviraient à rien
               dans une arène confinée. */
            m_viewMode = 1;
            /* Turbine et rotor déjà au régime, quel que soit le choix du menu :
               on entre directement dans le combat, pas de séquence de démarrage
               (~1 min) à subir face à la horde. */
            m_flight.turbine().forceRunning();
        }
    } else {
        m_combat.stop();
    }

    /* Cycle jour/nuit : après m_combat.start()/stop() ci-dessus, pour que la
       nuit figée d'une arène dédiée (voir applySunSchedule) puisse s'appliquer
       ou, en repartant sur une carte normale, soit bien réévaluée depuis la
       config plutôt que de garder le réglage de la carte précédente. */
    applySunSchedule();
}

} /* namespace artouste::app */
