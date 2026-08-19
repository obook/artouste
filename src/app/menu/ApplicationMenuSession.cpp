/*
 * ApplicationMenuSession.cpp
 * Application des choix du menu à la session en cours : carte, turbine, démo,
 * mode zombie, et rechargement si le gestionnaire de cartes a remanié le
 * disque.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <string>

namespace artouste::app {

void Application::applyMenuSession() {
    m_etatDemo.depuisMenu = false; /* remis à jour ci-dessous selon le choix du menu */
    /* Démo choisie au menu : elle se joue sur Arcachon. On charge donc ce terrain ici
       (startDemo le confirmera sans le recharger), puis on lance la chorégraphie plus
       bas ; la turbine et la vue de départ retenues par la démo priment. */
    if (m_menu.demo) {
        m_menu.terrain = "arcachon";
    }
    /* Terrain : rechargé s'il a changé (l'opération est coûteuse), mais AUSSI si
       ses options ont changé. Le gestionnaire de cartes a pu éteindre ses
       bâtiments ou ses tuiles pendant que la scène était en mémoire, et relancer
       la même carte doit en tenir compte : sans cela les bâtiments éteints
       restaient à l'écran. Dans tous les cas on repasse ensuite par resetToStart :
       il repose l'appareil ET remet à zéro les commandes mémorisées (collectif),
       l'assistance et l'aide au posé -- loadTerrain seul ne purge pas ces états,
       et un collectif resté haut ferait redécoller l'appareil tout seul sur la
       nouvelle carte. */
    if (!m_menu.terrain.empty()) {
        const bool carteChangee = m_menu.terrain != m_terrainName;
        const bool optionsChangees =
            !carteChangee && !m_terrainName.empty() &&
            !(optionsEffectives(m_assetsDir / "terrain" / m_terrainName) == m_optionsChargees);
        /* Un jeu de tuiles fabriqué ou supprimé ne se lit ni dans le nom de la
           carte ni dans son options.txt, alors qu'il change tout au chargement :
           le gestionnaire le signale par m_cartesRemaniees. */
        /* monuments.txt retouché à la main pendant que la scène était en mémoire :
           on compare sa date d'écriture à celle relevée au chargement. Caler une
           trentaine de monuments demande de reprendre ce fichier des dizaines de
           fois, et sans ce test il fallait relancer le simulateur à chaque essai
           pour en voir l'effet. */
        const bool monumentsChanges =
            !carteChangee && !m_terrainName.empty() &&
            dateMonuments(m_assetsDir / "terrain" / m_terrainName) != m_monumentsDate;
        if (carteChangee || optionsChangees || m_cartesRemaniees || monumentsChanges) {
            loadTerrain(m_menu.terrain);  /* remet lui-même le drapeau à faux */
        }
    }
    resetToStart();

    /* Assistance éteinte à chaque départ en vol. resetToStart ne peut pas s'en
       charger : il sert aussi aux touches R et X, où couper l'assistance
       surprendrait le pilote en plein vol. Sans cette ligne, l'interrupteur
       survivait au retour au menu et l'on repartait assisté sans l'avoir
       demandé. La démo n'est pas concernée, elle a son propre chemin (voir
       setRealFlyPhysicsEnabled dans ApplicationLoop.cpp). */
    m_assist.disable();

    /* Livrée de départ (armée de terre) : appliquée à chaque relance depuis le menu,
       le modèle 3D persistant gardant sinon la livrée du vol précédent. */
    if (m_loadedHeli) {
        m_loadedHeli->setLivery(m_livery);
    }

    /* Turbine et rotor selon le choix du menu : démarrés d'emblée, ou à froid. */
    if (m_menu.turbine == 1) {
        m_flight.turbine().forceRunning();
    } else {
        m_flight.turbine().stopNow();
    }

    /* On repart d'un état neutre : ni pause, ni panneau de confirmation, et une
       annonce de la tour à émettre. Ce dernier point compte surtout pour le mode
       zombie, lancé turbine chaude : le réarmement automatique attend que la
       turbine redescende sous la moitié du régime, ce qui n'arrive pas entre deux
       parties, si bien que seule la première du processus était annoncée. */
    m_paused = false;
    m_confirmReset = false;
    m_confirmDemo = false;
    resetRadioMessage();

    /* Démo demandée au menu : on lance la démonstration (elle repose l'appareil, force
       le démarrage rapide de la turbine et joue la chorégraphie). En sortir ramènera au
       menu (m_etatDemo.depuisMenu). Sinon, on part en vol libre, démo arrêtée. */
    if (m_menu.demo) {
        m_etatDemo.depuisMenu = true;
        startDemo();
    } else {
        m_demo.stop();
    }

    /* Mode zombie demandé au menu : démarre (ou arrête) la session de combat sur
       le terrain courant, comme au premier lancement (voir ApplicationScene.cpp
       ::initScene). Sans effet si la carte n'a pas de zombies.txt. */
    if (m_menu.combat) {
        m_combat.start(m_assetsDir / "terrain" / m_terrainName,
                       [this](float x, float z) { return m_terrain->heightAt(x, z); });
        if (m_combat.active()) {
            m_viewMode =
                1; /* vue bloquée en cockpit pendant le combat (voir ApplicationInput.cpp) */
            /* Confort de pilotage et pilote automatique coupés en arène, comme
               au premier lancement (voir ApplicationSceneConfig.cpp). */
            m_assist.disable();
            m_autoland.stop();
            /* Turbine et rotor déjà au régime, quel que soit le choix du menu (voir
               ApplicationScene::initScene, même logique au premier lancement). */
            m_flight.turbine().forceRunning();
        }
    } else {
        m_combat.stop();
    }

    /* Après m_combat.start()/stop() ci-dessus : réévalue le cycle jour/nuit depuis
       la config (ou la nuit figée d'une arène dédiée) -- indispensable ici aussi,
       sans quoi revenir d'une telle arène vers une carte normale garderait le
       temps figé pour le reste de la session (voir ApplicationScene::initScene). */
    applySunSchedule();
}

} /* namespace artouste::app */
