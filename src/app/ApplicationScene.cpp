/*
 * ApplicationScene.cpp
 * Mise en place de la scène : localisation du dossier des ressources, chargement
 * des shaders, des maillages procéduraux, du terrain réel, des bâtiments et du
 * modèle 3D de l'appareil, puis lecture de la configuration (terrain, démo,
 * démarrage immédiat).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

/* Sous Windows, pour retrouver le dossier du binaire (GetModuleFileNameW).
 * WIN32_LEAN_AND_MEAN et NOMINMAX évitent que <windows.h> tire des macros
 * min/max qui entreraient en conflit avec GLM et la bibliothèque standard. */
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

#include "app/Application.hpp"

#include "app/AppConstants.hpp"
#include "app/Config.hpp"
#include "input/InputSystem.hpp"
#include "render/Buildings.hpp"
#include "render/HelicopterModel.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Mesh.hpp"
#include "render/Model.hpp"
#include "render/ModelLoader.hpp"
#include "render/Primitives.hpp"
#include "render/Shader.hpp"
#include "render/Skybox.hpp"
#include "render/Terrain.hpp"
#include "util/Math.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#ifndef ARTOUSTE_ASSET_DIR
#define ARTOUSTE_ASSET_DIR "assets"
#endif

namespace artouste::app {

namespace {

/*
 * Décalage maximal (rad) de la position de parking du rotor par rapport à l'axe.
 * Tiré au hasard à chaque lancement, pour que la pale ne soit pas figée pile dans
 * l'axe (plus naturel). Petit, pour rester loin de la sortie d'échappement.
 */
constexpr float ROTOR_PARK_JITTER = 0.26f;  /* ~15 degrés */

/* Dossier contenant le binaire en cours d'exécution, ou un chemin vide si on ne
 * sait pas le déterminer. Sert à trouver les ressources installées à côté du
 * binaire (cas d'une release). Portable : GetModuleFileNameW sous Windows,
 * /proc/self/exe sous Linux. */
std::filesystem::path executableDir() {
    namespace fs = std::filesystem;
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        return fs::path(buf, buf + n).parent_path();
    }
#else
    std::error_code ec;
    const fs::path  exe = fs::canonical("/proc/self/exe", ec);
    if (!ec) {
        return exe.parent_path();
    }
#endif
    return fs::path();
}

/*
 * Localise le dossier des ressources, dans l'ordre : variable d'environnement,
 * puis dossier "assets" placé à côté de l'exécutable (version packagée), puis
 * chemin connu à la compilation (développement).
 */
std::filesystem::path resolveAssetDir() {
    namespace fs = std::filesystem;
    if (const char* env = std::getenv("ARTOUSTE_ASSETS")) {
        if (fs::exists(env)) {
            return fs::path(env);
        }
    }
    /* Ressources installées à côté du binaire (release) : prioritaires sur le
       chemin de compilation, qui n'existe que sur la machine de développement. */
    const fs::path exeDir = executableDir();
    if (!exeDir.empty()) {
        const fs::path local = exeDir / "assets";
        if (fs::exists(local)) {
            return local;
        }
    }
    if (fs::exists(ARTOUSTE_ASSET_DIR)) {
        return fs::path(ARTOUSTE_ASSET_DIR);
    }
    return fs::path("assets");
}

}  /* namespace */

void Application::initScene() {
    /* Décalage de parking du rotor, tiré au hasard à chaque lancement : la pale ne
     * se range pas pile dans l'axe, ce qui est plus naturel. On y place aussi l'angle
     * de départ du rotor, pour qu'il soit déjà à cette position au lancement. */
    std::mt19937                          rng(std::random_device{}());
    std::uniform_real_distribution<float> jitter(-ROTOR_PARK_JITTER, ROTOR_PARK_JITTER);
    m_parkOffset = jitter(rng);
    m_rotorAngle = m_parkOffset;

    const std::filesystem::path assets = resolveAssetDir();
    m_musicPath = assets / "music" / "demo.mp3";  /* musique jouée pendant la démo (optionnelle) */
    m_shader = std::make_unique<render::Shader>(assets / "shaders" / "basic.vert",
                                                assets / "shaders" / "basic.frag");
    m_modelShader = std::make_unique<render::Shader>(assets / "shaders" / "model.vert",
                                                     assets / "shaders" / "model.frag");
    m_terrainShader = std::make_unique<render::Shader>(assets / "shaders" / "terrain.vert",
                                                       assets / "shaders" / "terrain.frag");
    m_seaShader   = std::make_unique<render::Shader>(assets / "shaders" / "sea.vert",
                                                     assets / "shaders" / "sea.frag");
    m_skyShader   = std::make_unique<render::Shader>(assets / "shaders" / "sky.vert",
                                                     assets / "shaders" / "sky.frag");
    m_flatShader  = std::make_unique<render::Shader>(assets / "shaders" / "flat.vert",
                                                     assets / "shaders" / "flat.frag");
    m_shadowShader = std::make_unique<render::Shader>(assets / "shaders" / "shadow.vert",
                                                      assets / "shaders" / "shadow.frag");
    m_buildingShader = std::make_unique<render::Shader>(assets / "shaders" / "building.vert",
                                                        assets / "shaders" / "building.frag");
    m_sky         = std::make_unique<render::Skybox>();

    const auto discData = render::primitives::softDisc(6.0f, 48);
    m_shadowDisc        = std::make_unique<render::Mesh>(discData.vertices, discData.indices);

    /* Petite sphère unité, réutilisée (mise à l'échelle au dessin) pour le flash du
       strombo et la lueur de la tuyère. Le shader plat ne lit que la position. */
    const auto sphereData = render::primitives::sphere(1.0f, 12, 16, vec3{1.0f, 1.0f, 1.0f});
    m_glowSphere          = std::make_unique<render::Mesh>(sphereData.vertices, sphereData.indices);

    /* Hélipad de la zone de départ : disque gris très clair (presque blanc), anneau
       gris foncé et grand H rouge. Centré sur l'origine ; placé au départ à l'affichage. */
    const auto padData = render::primitives::helipad(7.0f, 48, vec3{0.90f, 0.90f, 0.92f},
                                                     vec3{0.20f, 0.20f, 0.22f},
                                                     vec3{0.80f, 0.10f, 0.10f});
    m_helipad          = std::make_unique<render::Mesh>(padData.vertices, padData.indices);

    /* Hélipad texturé fabriqué avec Blender (voir tools/helipad). S'il est présent,
       il remplace la version procédurale ci-dessus ; sinon on garde celle-ci. */
    const std::filesystem::path helipadModel = assets / "models" / "helipad" / "helipad.ac";
    if (std::filesystem::exists(helipadModel)) {
        render::Model pad = render::ModelLoader::load(helipadModel, {}, {});
        if (!pad.empty()) {
            m_helipadModel = std::make_unique<render::Model>(std::move(pad));
        }
    }

    /* Plan de mer : un grand quadrilatère horizontal qui s'étend jusqu'à l'horizon. */
    const vec3 up{0.0f, 1.0f, 0.0f};
    const std::vector<render::Vertex> seaVerts = {
        {{-SEA_HALF, SEA_LEVEL, -SEA_HALF}, up, SEA_COLOR, {0.0f, 0.0f}},
        {{SEA_HALF, SEA_LEVEL, -SEA_HALF}, up, SEA_COLOR, {0.0f, 0.0f}},
        {{SEA_HALF, SEA_LEVEL, SEA_HALF}, up, SEA_COLOR, {0.0f, 0.0f}},
        {{-SEA_HALF, SEA_LEVEL, SEA_HALF}, up, SEA_COLOR, {0.0f, 0.0f}},
    };
    const std::vector<unsigned int> seaIdx = {0, 1, 2, 0, 2, 3};
    m_sea = std::make_unique<render::Mesh>(seaVerts, seaIdx);

    /*
     * Terrain réel (relief IGN + orthophoto drapée). Le terrain à charger est un
     * sous-dossier de assets/terrain/ (par exemple "ossau" ou "cote-landes"),
     * choisi par la clé "terrain" du fichier de configuration. La variable
     * d'environnement ARTOUSTE_TERRAIN, si elle est définie, a la priorité (pratique
     * pour tester une autre map sans toucher au fichier).
     */
    const app::Config config = app::loadConfig(assets / "config.txt");

    /* Mode démo automatique : clé "demo" de la configuration, surchargée par la
       variable d'environnement ARTOUSTE_DEMO (prioritaire). */
    bool demoEnabled = config.demo;
    if (const char* env = std::getenv("ARTOUSTE_DEMO"); env != nullptr && env[0] != '\0') {
        demoEnabled = (env[0] != '0');
    }

    std::string        terrainName = config.terrain;
    if (const char* env = std::getenv("ARTOUSTE_TERRAIN"); env != nullptr && env[0] != '\0') {
        terrainName = env;
    }
    /* La démo se déroule sur le bassin d'Arcachon (Dune du Pilat à survoler). */
    if (demoEnabled && terrainName != "arcachon") {
        std::printf("[scène] mode démo : terrain forcé sur arcachon.\n");
        terrainName = "arcachon";
    }
    std::printf("[scène] terrain : %s\n", terrainName.c_str());
    const std::filesystem::path terrainDir = assets / "terrain" / terrainName;
    m_terrain = std::make_unique<render::Terrain>(terrainDir);

    /* Bâtiments 3D (BD TOPO extrudée) propres au terrain, posés sur le relief.
       Absents (fichier buildings.bin manquant) : rien n'est dessiné. */
    m_buildings = std::make_unique<render::Buildings>(terrainDir, *m_terrain);

    /*
     * Position de départ : posé à Fabrèges, le fond de vallée plat à l'entrée du
     * massif (lac de Fabrèges, station du téléphérique d'Artouste), face au relief.
     * Si le terrain réel est absent, on reste à l'origine sur le sol plat de repli.
     */
    if (m_terrain->textured()) {
        /* Point de départ : celui fourni par le terrain (calage), sinon Fabrèges
           par défaut. Négatif en X = ouest, positif en Z = sud. */
        const float START_X = m_terrain->hasStart() ? m_terrain->startX() : 158.0f;
        const float START_Z = m_terrain->hasStart() ? m_terrain->startZ() : -3119.6f;
        const float ground  = m_terrain->heightAt(START_X, START_Z);
        m_startPos          = vec3{START_X, ground, START_Z};
        /* L'appareil se gare mât rotor centré sur le H : son origine (que la
           physique place) est donc reculée de ROTOR_FORWARD_OFFSET le long de l'axe
           de départ (cap initial = +X, orientation identité au reset). */
        const float parkX = START_X - render::LoadedHelicopter::ROTOR_FORWARD_OFFSET;
        m_parkPos         = vec3{parkX, m_terrain->heightAt(parkX, START_Z), START_Z};
        m_flight.reset(m_parkPos);
    }

    /* Démarrage immédiat (gain de temps en test) : turbine et rotor d'emblée au
       régime, au lieu de la séquence de démarrage (~1 min). Activé par la clé
       `turbine_demarree` de la config, ou la variable d'environnement
       ARTOUSTE_TURBINE_DEMARREE (prioritaire). */
    bool turbineRunning = config.turbineRunning;
    if (const char* env = std::getenv("ARTOUSTE_TURBINE_DEMARREE"); env != nullptr && env[0] != '\0') {
        turbineRunning = (env[0] != '0');
    }
    /* En mode démo, c'est la démo qui pilote la turbine (démarrage rapide) : on
       ignore donc le démarrage immédiat éventuel. */
    if (turbineRunning && !demoEnabled) {
        m_flight.turbine().forceRunning();
        std::printf("[scène] démarrage immédiat : turbine et rotor au régime.\n");
    }

    m_helicopter = std::make_unique<render::HelicopterModel>();
    m_input      = std::make_unique<input::InputSystem>(m_window);
    m_hud.init(m_window);
    m_audio.init(assets / "models" / "Alouette-II" / "Sounds");

    /*
     * On utilise le vrai modèle FlightGear s'il est présent : le sous-ensemble
     * nécessaire est versionné dans le dépôt (le paquet FlightGear complet, lui,
     * reste local). Sinon on conserve l'hélicoptère dessiné par le code.
     */
    const std::filesystem::path modelsDir = assets / "models" / "Alouette-II" / "Models";
    if (std::filesystem::exists(modelsDir / "alouette.ac")) {
        auto loaded = std::make_unique<render::LoadedHelicopter>(modelsDir);
        if (loaded->loaded()) {
            m_loadedHeli = std::move(loaded);
            /* Livrée Gendarmerie appliquée d'emblée (état par défaut). */
            m_loadedHeli->setGendarmerieLivery(m_gendarmerieLivery);
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

    /* Mode démo demandé au lancement : on démarre la démonstration tout de suite. */
    if (demoEnabled) {
        std::printf("[scène] mode démo activé : démonstration automatique en boucle.\n");
        startDemo();
    }
}

}  /* namespace artouste::app */
