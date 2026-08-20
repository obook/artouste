/*
 * ApplicationSceneTerrain.cpp
 * Chargement d'une carte et choix du point d'apparition.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "input/Gamepad.hpp"

#include "render/Bc7.hpp"
#include "render/Buildings.hpp"
#include "render/Clouds.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"
#include "render/Vegetation.hpp"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

namespace artouste::app {

void Application::loadTerrain(const std::string& name) {
    m_terrainName = name;
    /* Le nuage de poussière porte des coordonnées monde : gardé d'une carte à
       l'autre, il réapparaîtrait n'importe où sur la nouvelle. */
    m_souffle.vider();
    /* Le terrain qu'on va bâtir reflétera le disque tel qu'il est maintenant,
       tuiles comprises : ce qu'a pu faire le gestionnaire de cartes est donc pris
       en compte, et le drapeau qui le signalait n'a plus lieu d'être. */
    m_cartesRemaniees = false;
    std::printf("[scène] terrain : %s\n", name.c_str());
    const std::filesystem::path terrainDir = m_assetsDir / "terrain" / name;

    /* Options propres à cette carte (options.txt facultatif) : elles priment sur
       la configuration générale, qui ne peut pas savoir qu'on veut des arbres en
       montagne et des bâtiments en ville. Lues AVANT le terrain : la fenêtre de
       tuiles se décide à sa construction. Mémorisées, pour savoir au retour du
       menu si elles ont changé et s'il faut recharger. */
    m_optionsChargees = optionsEffectives(terrainDir);
    const bool arbresIci = m_optionsChargees.arbres;
    const bool batimentsIci = m_optionsChargees.batiments;
    const bool tuilesIci = m_optionsChargees.tuiles;
    std::printf("[scène] options de la carte : arbres %s, bâtiments %s, tuiles %s\n",
                arbresIci ? "oui" : "non",
                batimentsIci ? "oui" : "non",
                tuilesIci ? "oui" : "non");

    /* Au tout premier chargement d'une carte, son orthophoto doit être
       compressée avant d'être mise en cache : une trentaine de secondes sur une
       carte fine, pendant lesquelles il faut montrer autre chose qu'une fenêtre
       figée. Les lancements suivants relisent le cache et n'appellent jamais ce
       rappel. Renvoyer faux annule la préparation : on le fait si l'utilisateur
       ferme la fenêtre, plutôt que de le retenir jusqu'au bout. */
    m_terrain = std::make_unique<render::Terrain>(
        terrainDir,
        [this](float fraction) {
            renderLoadingScreen("Préparation de la carte, une seule fois...", fraction);
            return glfwWindowShouldClose(m_window) == 0;
        },
        tuilesIci ? m_detailWindowPx : 0,
        m_reliefVertexBudget,
        racineTuiles(),
        m_reliefWindow);

    /* Chaque étape ci-dessous bloque plusieurs secondes sans rien lire. On vide donc
       la file de la manette entre elles : sinon elle déborde, le noyau jette des
       événements et un bouton relâché pendant le chargement reste vu comme enfoncé
       (voir Gamepad::viderFile). */
    input::Gamepad::viderFile();

    /* Bâtiments 3D (BD TOPO extrudée) propres au terrain, posés sur le relief.
       Absents (fichier buildings.bin manquant) ou refusés par la carte : rien
       n'est dessiné. */
    m_buildings =
        batimentsIci ? std::make_unique<render::Buildings>(terrainDir, *m_terrain) : nullptr;

    input::Gamepad::viderFile();

    /* Végétation en billboards : arbres semés d'après l'orthophoto, posés sur le
       relief. Activée par défaut ; désactivable par la clé "arbres 0" de config.txt
       (et toujours désactivée si ARTOUSTE_NO_TREES est défini) -- voir m_treesEnabled,
       calculé dans initScene. Atlas de sprites partagé entre terrains. */
    if (arbresIci) {
        m_vegetation = std::make_unique<render::Vegetation>(
            terrainDir, *m_terrain, m_assetsDir / "vegetation" / "trees_atlas.png", m_treeBudget);
    } else {
        m_vegetation.reset();
    }

    input::Gamepad::viderFile();

    /* Monuments 3D déclarés par la carte (monuments.txt) : modèles ponctuels posés
       à une coordonnée. Chargés après les bâtiments, dont ils ont fait dégager
       l'emprise (voir BuildingsMesh et le champ rayon_m). */
    loadMonuments();

    /* Nuages en billboards (prototype) : couche de cumulus épars au-dessus du relief.
       Partagent la texture de bouffée entre terrains. */
    m_clouds = std::make_unique<render::Clouds>(*m_terrain, m_assetsDir / "clouds" / "puff.png");

    /*
     * Position de départ : posé à Fabrèges, le fond de vallée plat à l'entrée du
     * massif (lac de Fabrèges, station du téléphérique d'Artouste), face au relief.
     * Si le terrain réel est absent, on reste à l'origine sur le sol plat de repli.
     */
    if (m_terrain->textured()) {
        /* Point de départ : on privilégie les hélipads réels (helipads.txt) comme aire
           de poser. Le repère du terrain (start_x/start_z, sinon Fabrèges par défaut)
           ne sert qu'à choisir LEQUEL : on cale le départ sur l'hélipad le plus proche
           de ce repère. helipads.txt reste ainsi la seule source de la position exacte,
           sans la dupliquer dans terrain.txt. Sans hélipad, on garde le repère brut.
           Négatif en X = ouest, positif en Z = sud. */
        float START_X = m_terrain->hasStart() ? m_terrain->startX() : 158.0f;
        float START_Z = m_terrain->hasStart() ? m_terrain->startZ() : -3119.6f;
        const float hintX = START_X;
        const float hintZ = START_Z;
        float bestD2 = -1.0f; /* sentinelle : aucun hélipad encore retenu */
        m_radio.stationDepart.clear();
        for (const render::Landmark& pad : m_terrain->helipads()) {
            float px = 0.0f, pz = 0.0f;
            m_terrain->worldAt(pad.lon, pad.lat, px, pz);
            const float d2 = (px - hintX) * (px - hintX) + (pz - hintZ) * (pz - hintZ);
            if (bestD2 < 0.0f || d2 < bestD2) {
                bestD2 = d2;
                START_X = px;
                START_Z = pz;
                m_radio.stationDepart = pad.name; /* station d'origine = hélipad de départ */
            }
        }
        const float ground = m_terrain->heightAt(START_X, START_Z);
        m_startPos = vec3{START_X, ground, START_Z};
        /* L'appareil se gare mât rotor centré sur le H : son origine (que la
           physique place) est donc reculée de ROTOR_FORWARD_OFFSET le long de l'axe
           de départ, donné par le cap initial de la carte (clé start_heading de
           terrain.txt ; 90 = est par défaut, l'orientation identité). */
        const float capRad = glm::radians(m_terrain->startHeadingDeg());
        const vec3 avant{std::sin(capRad), 0.0f, -std::cos(capRad)};
        const float parkX = START_X - avant.x * render::LoadedHelicopter::ROTOR_FORWARD_OFFSET;
        const float parkZ = START_Z - avant.z * render::LoadedHelicopter::ROTOR_FORWARD_OFFSET;
        m_parkPos = vec3{parkX, m_terrain->heightAt(parkX, parkZ), parkZ};
        m_flight.reset(m_parkPos, m_terrain->startHeadingDeg());

        /* Point d'apparition demandé sur la ligne de commande : on repose
           l'appareil ailleurs, après le placement au pad et non à sa place. Le
           pad reste ainsi défini (m_startPos, m_parkPos, m_radio.stationDepart), donc la
           touche R y ramène, l'atterrissage automatique le connaît et la radio
           annonce la bonne station. Seule la position de DÉPART change. */
        float lon = 0.0f;
        float lat = 0.0f;
        if (resoudrePointDapparition(lon, lat)) {
            float x = 0.0f;
            float z = 0.0f;
            m_terrain->worldAt(lon, lat, x, z);
            /* Hauteur comptée au-dessus du sol : 300 m par défaut, de quoi voir
               un monument en entier sans avoir à monter. */
            const float agl = m_options.aAltitude ? m_options.altitude : 300.0f;
            const float cap = m_options.aCap ? m_options.cap : m_terrain->startHeadingDeg();
            m_flight.reset(vec3{x, m_terrain->heightAt(x, z) + agl, z}, cap);
            std::printf("[scène] apparition à %.6f / %.6f, %.0f m sol, cap %.0f.\n",
                        static_cast<double>(lon), static_cast<double>(lat),
                        static_cast<double>(agl), static_cast<double>(cap));
        }
    }
}

bool Application::resoudrePointDapparition(float& lon, float& lat) const {
    if (!m_options.aPointDapparition() || m_terrain == nullptr || !m_terrain->hasGeo()) {
        return false;
    }
    /* Coordonnées explicites : rien à chercher. */
    if (m_options.aLonLat) {
        lon = m_options.lon;
        lat = m_options.lat;
        return true;
    }

    /* Recherche par nom, sur les noms normalisés (casse et accents ignorés, un
       fragment suffit) : taper "pantheon" au shell doit trouver "Panthéon". */
    const bool parMonument = !m_options.monument.empty();
    const std::string cherche =
        normaliserNom(parMonument ? m_options.monument : m_options.lieu);
    if (cherche.empty()) {
        return false;
    }
    if (parMonument) {
        for (const render::Monument& m : m_terrain->monuments()) {
            if (normaliserNom(m.name).find(cherche) != std::string::npos) {
                lon = m.lon;
                lat = m.lat;
                return true;
            }
        }
        std::printf("[scène] monument \"%s\" introuvable sur cette carte : départ au pad.\n",
                    m_options.monument.c_str());
        return false;
    }
    for (const render::Landmark& l : m_terrain->landmarks()) {
        if (normaliserNom(l.name).find(cherche) != std::string::npos) {
            lon = l.lon;
            lat = l.lat;
            return true;
        }
    }
    std::printf("[scène] lieu \"%s\" introuvable sur cette carte : départ au pad.\n",
                m_options.lieu.c_str());
    return false;
}

} /* namespace artouste::app */
