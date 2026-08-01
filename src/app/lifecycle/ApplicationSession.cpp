/*
 * ApplicationSession.cpp
 * Actions de session : livrée de l'appareil, retour au pad de départ et
 * lancement de la démonstration automatique. Extrait de Application.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "input/InputSystem.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"

#include <cstdio>
#include <vector>

namespace artouste::app {

void Application::cycleLivery() {
    if (!m_loadedHeli) {
        return;
    }
    switch (m_livery) {
        case render::Livery::Blanche:          m_livery = render::Livery::Gendarmerie;      break;
        case render::Livery::Gendarmerie:      m_livery = render::Livery::ArmeeDeTerre;     break;
        case render::Livery::ArmeeDeTerre:     m_livery = render::Livery::ProtectionCivile; break;
        case render::Livery::ProtectionCivile: m_livery = render::Livery::Blanche;          break;
    }
    m_loadedHeli->setLivery(m_livery);
}

void Application::toggleAutoland() {
    /* Déjà engagé : on rend la main tout de suite, comme le mode assisté (touche M). */
    if (m_autoland.active()) {
        m_autoland.stop();
        return;
    }
    vec3 posePad{0.0f, 0.0f, 0.0f};
    if (padPlusProche(m_flight.body().position, posePad) != nullptr) {
        m_autoland.start(posePad, m_lastControls);
        /* Engagement réussi : efface tout de suite un message d'échec resté à
           l'écran (interruption précédente, aucun pad trouvé la fois d'avant),
           sinon il reste affiché jusqu'à expiration de son délai alors que
           l'atterrissage automatique est de nouveau actif. */
        m_autolandMsgShow = 0.0f;
        return;
    }
    /* Aucun pad dans le rayon de recherche (voir padPlusProche) : rien à engager. Sans
       message, le joueur presse la touche et ne voit rien se passer, sans comprendre
       pourquoi (voir aussi le message d'auto-désengagement, ApplicationLoop.cpp). */
    m_autolandMsg     = "Aucun pad à portée";
    m_autolandMsgShow = 3.0f;
}

void Application::resetToStart() {
    m_flight.reset(m_parkPos, m_terrain->startHeadingDeg());
    m_input->reset();
    /* L'appareil est téléporté au pad : la poussière qu'il soulevait là où il
       était resterait suspendue en l'air, loin derrière lui. */
    m_souffle.vider();
    /* On remet aussi à zéro les états qui gardent une mémoire du collectif : les
       dernières commandes (réutilisées pendant un panneau figé, assistance OFF) et
       l'état lissé de l'assistance (assistance ON). Sans cela, le collectif mémorisé
       subsiste après un retour au pad et l'appareil redécolle tout seul. */
    m_lastControls = physics::Controls{};
    m_assist.reset();
    m_autoland.stop();
    m_confirmReset = false;
    /* Retour au pad : on repart d'un état d'aide au posé vierge, sinon le réticule
       (ou un score en cours d'affichage) resterait visible alors qu'on est de nouveau
       garé sans avoir volé. m_hasFlown se rearmera au prochain décollage. */
    m_hasFlown      = false;
    m_wasAirborne   = false;
    m_wasOnGround   = false;
    m_scoreTimer    = 0.0f;
    m_padGuideGrace = 0.0f;  /* le prochain (premier) décollage réarmera le délai */
}

void Application::startDemo() {
    /* La démo se déroule sur le bassin d'Arcachon (Dune du Pilat en altitude puis cap
       Ferret en rase-mottes). Si une autre carte est affichée (choix du menu), on bascule
       d'abord sur Arcachon : sinon la démo se jouerait sur la carte courante, sans ces
       lieux à survoler. */
    if (m_terrainName != "arcachon") {
        std::printf("[démo] terrain forcé sur arcachon pour la démonstration.\n");
        loadTerrain("arcachon");
    }

    /* Pad de départ et d'arrivée : la position de parking (m_parkPos), où l'appareil est
       garé mât rotor centré sur le H. On vise cette position au retour (et non le centre
       du H, m_startPos) pour que la pose recentre le mât sur le H exactement comme au
       décollage, sans décalage de ROTOR_FORWARD_OFFSET. */
    const vec3 returnPad = m_parkPos;

    /* Route de la démo (voir ROADMAP.md, section Mode demo) : Dune du Pilat à 1000 m
       (panorama), puis cap Ferret en rase-mottes à 30 m, avant de faire
       demi-tour et de revenir se poser au pad. Route courte volontairement, centrée sur
       les deux temps forts. Sans terrain, la route reste vide : la démo se contente alors
       d'un décollage suivi d'une pose. */
    std::vector<DemoPilot::Waypoint> route;
    if (m_terrain != nullptr) {
        /* Ajoute un point de passage à partir de coordonnées géographiques (lon/lat),
           converties en position monde, avec sa hauteur de survol. */
        auto ajouter = [&](float lon, float lat, float altitude) {
            float x = 0.0f;
            float z = 0.0f;
            m_terrain->worldAt(lon, lat, x, z);
            route.push_back(
                DemoPilot::Waypoint{vec3{x, m_terrain->heightAt(x, z), z}, altitude});
        };
        /* Dune du Pilat puis cap Ferret : coordonnées explicites des points de survol.
           La démo fait demi-tour au cap Ferret et revient se poser au pad. */
        ajouter(-1.2020697f, 44.5912130f, 1000.0f);  /* Dune du Pilat (panorama) */
        ajouter(-1.2450709f, 44.6184674f, 30.0f);    /* cap Ferret (rase-mottes le long de la côte) */
    }

    /* On repart d'un état propre : appareil sur le pad, turbine à froid, puis on
       lance le démarrage rapide et la chorégraphie. */
    resetToStart();
    m_viewMode = 2;  /* vue d'orbite pour le démarrage */
    m_flight.turbine().stopNow();
    m_flight.turbine().startFast();
    m_demoUserView = false;  /* la démo reprend la main sur la vue et le HUD */
    m_demoUserHud  = false;
    m_demoInputGraceS = 0.6f;  /* ignore un résidu d'entrée pilote juste après le lancement */
    m_demo.start(returnPad, route);
    /* Musique de la démo mise de côté pour l'instant : on ne la lance pas.
       (Tout le mécanisme reste en place ; décommenter pour la réactiver.) */
    /* m_audio.playMusic(m_musicPath); */
}

}  /* namespace artouste::app */
