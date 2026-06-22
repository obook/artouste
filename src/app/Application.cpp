/*
 * Application.cpp
 * Implémentation du cycle de vie du simulateur : ouverture de la fenêtre,
 * chargement d'OpenGL, construction de la scène, boucle principale (entrées,
 * physique à pas fixe, rendu, audio) et capture d'écran.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

/* Sous Windows, pour retrouver le dossier du binaire (GetModuleFileNameW).
 * WIN32_LEAN_AND_MEAN et NOMINMAX evitent que <windows.h> tire des macros
 * min/max qui entreraient en conflit avec GLM et la bibliotheque standard. */
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

#include "app/Application.hpp"

#include <glad/glad.h>
/* glad doit précéder GLFW. */
#include <GLFW/glfw3.h>

#include "app/Clock.hpp"
#include "app/Config.hpp"
#include "input/InputSystem.hpp"
#include "physics/RigidBody.hpp"
#include "render/HelicopterModel.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Mesh.hpp"
#include "render/Model.hpp"
#include "render/ModelLoader.hpp"
#include "render/Primitives.hpp"
#include "render/Shader.hpp"
#include "render/Buildings.hpp"
#include "render/Skybox.hpp"
#include "render/Terrain.hpp"
#include "util/Math.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <random>
#include <vector>

#ifndef ARTOUSTE_ASSET_DIR
#define ARTOUSTE_ASSET_DIR "assets"
#endif

namespace artouste::app {

namespace {

constexpr int  WINDOW_WIDTH  = 1280;
constexpr int  WINDOW_HEIGHT = 720;
constexpr char WINDOW_TITLE[] = "Artouste";

/*
 * Position de l'oeil du pilote (siège droit) dans le repère corps : avant (X),
 * haut (Y), droite (Z). Réglée pour cadrer la planche de bord et la verrière, et
 * pour laisser voir les avant-bras du pilote sur le manche en partie basse (le
 * torse et le haut des bras, eux, sont retirés du modèle en vue cockpit, voir
 * LoadedHelicopter). Posée sur le siège droit, un peu recentrée, et avancée pour
 * rapprocher la planche de bord.
 */
const vec3 COCKPIT_EYE{3.55f, 1.86f, 0.46f};

/*
 * Repères des effets moteur, dans le même repère corps que COCKPIT_EYE (X avant,
 * Y haut, Z droite, origine au centre de l'appareil). Réglés à l'oeil via le mode
 * capture (ARTOUSTE_SHOT_*).
 *
 * Strombo (feu anti-collision) : petite sphère rouge posée au-dessus de la cabine,
 * qui clignote (allumée une brève fraction de la période) tant que la turbine tourne.
 */
const vec3        BEACON_POS{3.11f, 2.41f, 0.0f};  /* position du beacon du modèle (all-lights.xml), sur le toit */
constexpr float   BEACON_RADIUS = 0.10f;           /* rayon de la sphère (m) */
constexpr float   BEACON_PERIOD = 1.2f;            /* période du clignotement (s) */
constexpr float   BEACON_ON     = 0.18f;           /* fraction de la période où le feu est allumé */

/*
 * Feux de navigation et de queue, aux positions du modèle (all-lights.xml d'Émmanuel),
 * exprimées ici en repère corps. Allumés en continu quand la turbine tourne. On respecte
 * la convention aéronautique : rouge à bâbord (gauche), vert à tribord (droite), blanc à
 * la queue.
 */
const vec3        NAV_LEFT_POS{4.24f, 0.92f, -0.77f};   /* feu de navigation bâbord (rouge) */
const vec3        NAV_RIGHT_POS{4.24f, 0.92f, 0.77f};   /* feu de navigation tribord (vert) */
const vec3        TAIL_LIGHT_POS{-3.96f, 2.13f, 0.20f}; /* feu blanc de queue */
constexpr float   NAV_RADIUS = 0.07f;                   /* rayon du coeur d'un feu (m) */

/*
 * Tuyère : sortie de la turbine, derrière le bloc moteur, au départ de la poutre.
 * La turbine est haute sur le pont moteur (derrière le mât) : la sortie est donc en
 * arrière de l'origine (X négatif) et en hauteur, pas dans le carter (plus bas, plus
 * en avant). On ne dessine pas de flamme : la turbine rejette de l'air très chaud,
 * qui se traduit par une distorsion thermique localisée (léger halo bleuté qui ondule)
 * et un petit coeur tiède sur le métal de la tuyère.
 */
const vec3        NOZZLE_BODY_POS{-0.30f, 2.28f, 0.0f};
constexpr float   NOZZLE_RADIUS = 0.24f;

/*
 * Rotor principal (animation visuelle) :
 *   - ROTOR_SPIN_RATE : vitesse de rotation à plein régime, en rad/s. Volontairement
 *     ralentie par rapport à la réalité pour éviter l'effet stroboscopique.
 *   - BLADE_SPACING : écart entre deux pales (rotor tripale -> 120 degrés). Les
 *     positions de parking sont les multiples de cet écart : une pale alignée sur
 *     l'axe de l'appareil, les deux autres encadrant la sortie d'échappement.
 *   - PARK_TAU : constante de temps du retour en position de parking, à l'arrêt.
 *   - ROTOR_PARK_JITTER : décalage maximal (rad) de la position de parking par
 *     rapport à l'axe. Tiré au hasard à chaque lancement, pour que la pale ne soit
 *     pas figée pile dans l'axe (plus naturel). Petit, pour rester loin de la sortie
 *     d'échappement.
 */
constexpr float ROTOR_SPIN_RATE   = 16.0f;
constexpr float BLADE_SPACING     = 2.0944f;  /* 2*pi/3 rad ~ 120 degrés */
constexpr float PARK_TAU          = 0.6f;
constexpr float ROTOR_PARK_JITTER = 0.26f;    /* ~15 degrés */

/*
 * Brume atmosphérique : le terrain et la mer se fondent dans cette teinte de
 * ciel entre FOG_START et FOG_END (distance à la caméra, en mètres). Elle donne
 * la profondeur et masque le bord du terrain comme le plan de coupe lointain.
 */
const vec3      FOG_COLOR{0.74f, 0.80f, 0.86f};
constexpr float FOG_START = 4000.0f;
constexpr float FOG_END   = 22000.0f;

/*
 * Plan de mer : couleur de l'océan (accordée à la mer recolorée de l'orthophoto)
 * et demi-côté en mètres. Il est au même niveau que la mer du bloc de terrain
 * (dessinée à y=0 par l'orthophoto), pour un raccord net au bord du terrain : sans
 * cela, un plan plus bas laissait une marche visible (jointure en escalier) le long
 * du bord. Pas de z-fighting malgré ce niveau commun car ce plan est dessiné sans
 * écrire dans le tampon de profondeur (voir renderScene), donc le terrain le
 * recouvre toujours ; il ne se voit qu'au-delà du bord du terrain, jusqu'à l'horizon.
 */
const vec3      SEA_COLOR{0.180f, 0.259f, 0.271f};  /* (46,66,69) : mer de l'orthophoto */
constexpr float SEA_LEVEL = 0.0f;
constexpr float SEA_HALF  = 100000.0f;

void glfwErrorCallback(int code, const char* description) {
    std::fprintf(stderr, "[GLFW] erreur %d : %s\n", code, description);
}

/*
 * Localise le dossier des ressources, dans l'ordre : variable d'environnement,
 * puis chemin connu à la compilation (développement), puis dossier "assets"
 * placé à côté de l'exécutable (version packagée).
 */
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

Application::Application() = default;

Application::~Application() {
    /*
     * Les ressources GL (Shader, Mesh) doivent être détruites tant que le
     * contexte OpenGL existe encore : on les libère donc avant de fermer GLFW.
     */
    m_hud.shutdown();
    m_input.reset();
    m_loadedHeli.reset();
    m_helicopter.reset();
    m_buildings.reset();
    m_terrain.reset();
    m_sea.reset();
    m_shadowDisc.reset();
    m_glowSphere.reset();
    m_helipad.reset();
    m_helipadModel.reset();
    m_sky.reset();
    m_flatShader.reset();
    m_shadowShader.reset();
    m_buildingShader.reset();
    m_skyShader.reset();
    m_seaShader.reset();
    m_terrainShader.reset();
    m_modelShader.reset();
    m_shader.reset();

    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

bool Application::initWindow() {
    glfwSetErrorCallback(glfwErrorCallback);

    if (glfwInit() != GLFW_TRUE) {
        std::fprintf(stderr, "Impossible d'initialiser GLFW.\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (m_window == nullptr) {
        std::fprintf(stderr, "Création de la fenêtre GLFW échouée.\n");
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);  /* synchronisation verticale (vsync) */

    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetFramebufferSizeCallback(m_window, resizeCallback);
    return true;
}

bool Application::initGL() {
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
        std::fprintf(stderr, "Échec du chargement OpenGL via GLAD.\n");
        return false;
    }

    std::printf("OpenGL  : %s\n", glGetString(GL_VERSION));
    std::printf("GLSL    : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    std::printf("Renderer: %s\n", glGetString(GL_RENDERER));

    glfwGetFramebufferSize(m_window, &m_width, &m_height);
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    return true;
}

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

void Application::mainLoop() {
    constexpr float SIM_DT = 1.0f / 240.0f;  /* pas fixe de simulation (240 Hz) */

    Clock clock;
    float accumulator = 0.0f;

    while (glfwWindowShouldClose(m_window) == GLFW_FALSE) {
        glfwPollEvents();
        clock.tick();
        const float t = static_cast<float>(clock.elapsed());
        /*
         * On borne le pas de temps : cela évite un saut brutal à la première
         * image ou après un gel (débogage, fenêtre déplacée).
         */
        const float frameDt = clamp(static_cast<float>(clock.dt()), 0.0f, 0.1f);

        /*
         * Entrées -> commandes, puis intégration de la physique à pas fixe,
         * indépendante de la cadence de rendu.
         */
        /* Commandes brutes du pilote (manche, palonnier, collectif). */
        const physics::Controls rawInput = m_input->poll(frameDt);

        /* Mode démo : une vraie action du pilote (manche, palonnier ou collectif)
         * lui rend la main et coupe la démo. */
        if (m_demo.active()) {
            const float pilotInput = std::fabs(rawInput.cyclicLateral)
                                   + std::fabs(rawInput.cyclicLongitudinal)
                                   + std::fabs(rawInput.pedals)
                                   + std::fabs(rawInput.collective);
            if (pilotInput > 0.15f) {
                m_demo.stop();
            }
        }

        /* Commandes effectives : fournies par le pilote automatique en mode démo,
         * sinon les commandes du pilote passées par le mode assisté (qui les corrige
         * et les adoucit s'il est actif, sinon les laisse intactes). */
        physics::Controls controls;
        if (m_demo.active()) {
            const physics::RigidBody& demoBody = m_flight.body();
            const vec3  demoFwd    = demoBody.orientation * vec3{1.0f, 0.0f, 0.0f};
            const float demoHeading = std::atan2(-demoFwd.z, demoFwd.x);
            const float demoGround  = m_terrain->heightAt(demoBody.position.x, demoBody.position.z);
            const DemoPilot::Output demoOut =
                m_demo.update(frameDt, demoBody.position, demoBody.velocity, demoHeading, demoGround,
                              m_flight.turbine().rotorFraction());
            controls = demoOut.controls;
            /* Fin de vol : la démo demande de couper la turbine une fois posée. */
            if (demoOut.cutTurbine) {
                m_flight.turbine().toggle();
            }
            m_viewMode = demoOut.viewMode;
            /* Démo : pas de HUD de vol (les étiquettes des lieux restent affichées, voir
               le rendu du HUD). */
            m_hudMode = ui::HudMode::Off;
            if (demoOut.finished) {
                startDemo();  /* la démo est terminée : on la rejoue en boucle */
            }
        } else {
            controls = m_assist.apply(rawInput, frameDt);
        }

        /* Musique de la démo : coupée dès que la démo s'arrête (entrée pilote, touche V,
           etc.). Le lancement, lui, se fait dans startDemo(). */
        const bool demoActiveNow = m_demo.active();
        if (m_demoWasActive && !demoActiveNow) {
            m_audio.stopMusic();
        }
        m_demoWasActive = demoActiveNow;

        /* Boutons et touches d'action de la manette : neutralisés pendant la démo
         * (le pilote la coupe en touchant les commandes de vol, ou avec la touche V). */
        if (m_demo.active()) {
            /* Rien : la démo ignore les boutons d'action. */
        } else if (m_input->assistTogglePressed()) {  /* croix haut : mode assisté (touche M) */
            m_assist.toggle();
        }

        /* Bouton Y de la manette : change de vue, comme la touche C du clavier. */
        if (!m_demo.active() && m_input->viewTogglePressed()) {
            m_viewMode = (m_viewMode + 1) % 3;
        }

        /* Bouton Start de la manette : démarre ou coupe la turbine, comme la touche T. */
        if (!m_demo.active() && m_input->turbineTogglePressed()) {
            m_flight.turbine().toggle();
        }

        /* Boutons manette équivalents aux touches clavier H, P, R et Échap, pour
         * pouvoir jouer à la manette seule. */
        if (m_demo.active()) {
            /* Aucune action manette pendant la démo. */
        } else if (m_confirmReset) {
            /* Panneau de confirmation affiché : A = Oui, B = Non. Les autres actions
             * des boutons sont neutralisées tant qu'on n'a pas répondu. */
            if (m_input->liveryTogglePressed()) {  /* A : Oui -> reset */
                resetToStart();
            } else if (m_input->hudTogglePressed()) {  /* B : Non -> on annule */
                m_confirmReset = false;
            }
        } else if (m_confirmDemo) {
            /* Panneau de confirmation de la démo : A = Oui (on lance), B = Non. */
            if (m_input->liveryTogglePressed()) {
                m_confirmDemo = false;
                startDemo();
            } else if (m_input->hudTogglePressed()) {
                m_confirmDemo = false;
            }
        } else {
            if (m_input->hudTogglePressed()) {  /* B : fait défiler les modes HUD (comme H) */
                m_hudMode = static_cast<ui::HudMode>((static_cast<int>(m_hudMode) + 1) % 3);
            }
            if (m_input->pauseTogglePressed()) {  /* Back : pause/reprise (comme P) */
                m_paused = !m_paused;
            }
            if (m_input->resetPressed()) {  /* X : demande la confirmation du reset (comme R) */
                m_confirmReset = true;
            }
            if (m_input->quitPressed()) {  /* LB + RB : quitte (comme Échap) */
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            }
            if (m_input->liveryTogglePressed()) {  /* A : bascule la livrée (comme L) */
                toggleGendarmerieLivery();
            }
        }

        /* Hauteur du relief sous l'appareil : sert au contact avec le sol. */
        const vec3& pos = m_flight.body().position;
        m_flight.setGroundHeight(m_terrain->heightAt(pos.x, pos.z));

        /* État physique avant le dernier pas, conservé pour interpoler le rendu. */
        physics::RigidBody prevBody = m_flight.body();
        if (m_paused || m_confirmReset || m_confirmDemo) {  /* un panneau de confirmation fige le vol */
            accumulator = 0.0f;  /* pas de rattrapage à la reprise */
        } else {
            accumulator += frameDt;
            while (accumulator >= SIM_DT) {
                prevBody = m_flight.body();
                m_flight.update(controls, SIM_DT);
                accumulator -= SIM_DT;
            }
        }

        const physics::RigidBody& body = m_flight.body();

        /* Interpolation entre l'avant-dernier et le dernier état physique, selon le
         * reste de l'accumulateur. La physique tourne à pas fixe (240 Hz) et le rendu
         * à une autre cadence : sans cette interpolation, l'appareil avance d'un
         * nombre variable de pas par image, ce qui se voit comme un sautillement,
         * surtout en vue de poursuite. */
        const float alpha     = accumulator / SIM_DT;
        const vec3  renderPos = glm::mix(prevBody.position, body.position, alpha);
        const quat  renderOri = glm::slerp(prevBody.orientation, body.orientation, alpha);

        /* Transformation monde de l'appareil : translation + orientation (quaternion). */
        const mat4 base =
            glm::translate(mat4(1.0f), renderPos) * glm::mat4_cast(renderOri);

        /* Cap (lacet) extrait de l'orientation, utile pour la caméra de poursuite. */
        const vec3  forward = renderOri * vec3{1.0f, 0.0f, 0.0f};
        const float yaw     = std::atan2(-forward.z, forward.x);

        /* Caméra : poursuite, cockpit (solidaire de l'appareil) ou orbite libre.
           Le changement de vue est un cut net : on remet à zéro le lissage de la
           poursuite pour qu'elle se cale instantanément, sans glissement. */
        if (m_viewMode != m_prevCamView) {
            if (m_prevCamView >= 0) {
                m_camera.cut();
            }
            if (m_viewMode == 2) {
                m_orbitStart = t;  /* début d'un segment orbite (pour le tour complet en démo) */
            }
            m_prevCamView = m_viewMode;
        }

        const vec3 lookTarget = renderPos + vec3{0.0f, 1.2f, 0.0f};
        if (m_viewMode == 1) {  /* cockpit */
            const vec3 eye = vec3(base * vec4(COCKPIT_EYE, 1.0f));
            const vec3 fwd = mat3(base) * glm::normalize(vec3{1.0f, -0.22f, 0.0f});
            const vec3 up  = mat3(base) * vec3{0.0f, 1.0f, 0.0f};
            m_camera.setFovYDeg(70.0f);
            m_camera.setNear(0.05f);  /* petit : ne tranche pas la verrière toute proche */
            m_camera.setLookAt(eye, eye + fwd, up);
        } else if (m_viewMode == 2) {  /* orbite */
            m_camera.setFovYDeg(60.0f);
            m_camera.setNear(0.5f);
            /* En démo, la caméra fait un tour complet (360 deg) autour de l'appareil
               sur la durée du segment orbite ; en pilotage manuel, rotation lente
               continue. (DEMO_ORBIT_TURN doit valoir la durée du segment orbite de la
               démo, voir DemoPilot.) */
            constexpr float DEMO_ORBIT_TURN = 14.0f;  /* s pour un tour complet en démo */
            const float angle = m_demo.active()
                                    ? (t - m_orbitStart) * (TWO_PI / DEMO_ORBIT_TURN)
                                    : t * 0.25f;
            m_camera.orbit(lookTarget, 15.0f, 6.0f, angle);
        } else {  /* poursuite */
            m_camera.setFovYDeg(60.0f);
            m_camera.setNear(0.5f);
            m_camera.chase(lookTarget, yaw, frameDt);
        }

        const float airspeed         = glm::length(vec2{body.velocity.x, body.velocity.z});
        const float turbineFraction  = m_flight.turbine().turbineFraction();
        const float rotorFraction    = m_flight.turbine().rotorFraction();

        /* Son ponctuel de démarrage : déclenché quand la turbine entre en phase de
         * démarrage, coupé si le pilote l'interrompt (passage en extinction). */
        const physics::Turbine::State turbineState = m_flight.turbine().state();
        if (turbineState != m_prevTurbineState) {
            if (turbineState == physics::Turbine::State::Demarrage) {
                m_audio.playStartSound();
            } else if (turbineState == physics::Turbine::State::Extinction) {
                m_audio.stopStartSound();
            }
            m_prevTurbineState = turbineState;
        }

        const audio::AudioEngine::View audioView =
            m_viewMode == 1 ? audio::AudioEngine::View::Interior   /* cockpit */
            : m_viewMode == 2 ? audio::AudioEngine::View::Fly      /* orbite */
                              : audio::AudioEngine::View::Rear;    /* poursuite */

        /* Effet Doppler : uniquement en vue extérieure libre (orbite). On le déduit
         * de la vitesse propre de l'appareil projetée sur l'axe caméra->appareil (la
         * caméra d'orbite est quasi fixe), et non d'une différence de distance entre
         * deux images : ainsi un changement de vue ou un reset, qui téléporte la
         * caméra, ne crée aucun Doppler parasite (la vitesse reste bornée). En vues
         * intérieure et poursuite, la caméra suit l'appareil -> pas de mouvement
         * relatif, pas de Doppler. Un léger lissage adoucit l'entrée/sortie d'effet. */
        float targetClosing = 0.0f;
        if (audioView == audio::AudioEngine::View::Fly) {
            const vec3  toCam = m_camera.position() - body.position;
            const float dist  = glm::length(toCam);
            if (dist > 0.001f) {
                targetClosing = glm::dot(body.velocity, toCam / dist);
            }
        }
        m_closingSpeed += (targetClosing - m_closingSpeed) * (1.0f - std::exp(-frameDt / 0.25f));

        /* En pause, on suspend les boucles sonores ; sinon on les module normalement. */
        m_audio.setPaused(m_paused);
        if (!m_paused) {
            m_audio.update(controls.collective, airspeed, turbineFraction, rotorFraction,
                           audioView, m_closingSpeed);
        }

        /* Angle du rotor principal. Il n'avance qu'au prorata du régime rotor (donc
         * pales immobiles turbine coupée, puis accélération), dans le sens horaire vu
         * de dessus (angle décroissant). À l'arrêt, on le ramène en douceur à la
         * position de parking la plus proche : une pale presque dans l'axe de l'appareil
         * (à un léger décalage aléatoire près, voir m_parkOffset), les deux autres de
         * part et d'autre de la sortie de la turbine, pour qu'aucune ne stationne dans
         * le jet chaud. Figé en pause, comme le reste de la simulation. */
        if (!m_paused) {
            if (rotorFraction > 0.0f) {
                m_rotorAngle -= rotorFraction * ROTOR_SPIN_RATE * frameDt;
            } else {
                /* Multiple de 120° le plus proche, décalé du jitter de parking, pour
                 * que la pale ne soit pas figée pile dans l'axe. */
                const float park =
                    std::round((m_rotorAngle - m_parkOffset) / BLADE_SPACING) * BLADE_SPACING
                    + m_parkOffset;
                const float ease = 1.0f - std::exp(-frameDt / PARK_TAU);
                m_rotorAngle += (park - m_rotorAngle) * ease;
            }
        }

        renderScene(base, m_rotorAngle, m_flight.turbine().rotorFraction(), controls.pedals,
                    controls.cyclicLongitudinal, controls.cyclicLateral, controls.collective,
                    turbineFraction, t);

        ui::HudData hud;
        hud.altitudeM    = body.position.y;
        hud.airspeedKt   = airspeed * 1.94384f;
        /* Cap boussole pour le HUD (ruban de cap, texte HDG, fleche de la minimap) :
           0 = nord, 90 = est, sens horaire. Le repere monde a X vers l'est et Z vers
           le sud, donc le nord est -Z. (Le lacet 'yaw' ci-dessus, mesure depuis l'est,
           reste reserve a la camera de poursuite.) */
        float headingDeg = glm::degrees(std::atan2(forward.x, -forward.z));
        if (headingDeg < 0.0f) {
            headingDeg += 360.0f;
        }
        hud.headingDeg    = headingDeg;
        hud.varioFpm      = body.velocity.y * 196.85f;
        hud.varioMs       = body.velocity.y;
        hud.collectivePct = controls.collective * 100.0f;
        hud.rotorPct      = rotorFraction * 100.0f;          /* régime rotor, en pourcentage */
        hud.rotorRpm      = rotorFraction * 360.0f;          /* régime rotor nominal : 360 tr/min */
        hud.turbineRpm    = turbineFraction * 33500.0f;      /* régime turbine nominal : ~33 500 tr/min */
        hud.exhaustTempC  = m_flight.turbine().exhaustTempC();  /* température tuyère (T4) */
        hud.fuelLiters    = m_flight.fuelLiters();
        hud.turbine       = m_flight.turbine().label();
        hud.assist        = m_assist.active();
        if (m_terrain->hasGeo()) {  /* longitude / latitude de l'appareil */
            float lon = 0.0f, lat = 0.0f;
            m_terrain->lonLatAt(body.position.x, body.position.z, lon, lat);
            hud.geoValid = true;
            hud.lonDeg   = lon;
            hud.latDeg   = lat;
        }
        buildNavHud(hud, body.position, headingDeg);
        /* En démo, le HUD est éteint mais on garde les étiquettes des lieux. */
        m_hud.render(hud, m_hudMode, m_paused, m_confirmReset, m_confirmDemo, m_demo.active());

        glfwSwapBuffers(m_window);
    }
}

void Application::renderScene(const mat4& base, float rotorAngle, float rotorFraction,
                              float rudder, float cyclicLong, float cyclicLat,
                              float collective, float turbineFraction, float timeSeconds) {
    const vec3 lightDir = glm::normalize(vec3{0.4f, 1.0f, 0.35f});
    const mat4 view     = m_camera.view();
    const mat4 proj     = m_camera.proj();

    glClearColor(0.53f, 0.70f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Ciel en dégradé (il remplit le fond de l'image). */
    m_sky->draw(*m_skyShader, glm::inverse(proj * view), m_camera.position());

    /* Plan de mer : grand quadrilatère bleu qui se perd dans la brume au loin.
     * Il est toujours sous la mer du terrain (dessinée à y=0) et n'a jamais à
     * occulter le terrain ; on le dessine donc sans écrire dans le tampon de
     * profondeur, de sorte que le terrain (dessiné après) le recouvre toujours.
     * Cela supprime le z-fighting au loin, y compris en vue cockpit où le faible
     * plan rapproché (near) dégrade fortement la précision de profondeur. */
    /* En montagne (terrain sans mer), on ne dessine pas le plan de mer. */
    if (m_terrain->drawsSea()) {
        m_seaShader->use();
        m_seaShader->setMat4("u_view", view);
        m_seaShader->setMat4("u_proj", proj);
        m_seaShader->setMat4("u_model", mat4(1.0f));
        m_seaShader->setVec3("u_seaColor", SEA_COLOR);
        m_seaShader->setVec3("u_lightDir", lightDir);
        m_seaShader->setVec3("u_camPos", m_camera.position());
        m_seaShader->setVec3("u_fogColor", FOG_COLOR);
        m_seaShader->setFloat("u_fogStart", FOG_START);
        m_seaShader->setFloat("u_fogEnd", FOG_END);
        glDepthMask(GL_FALSE);
        m_sea->draw();
        glDepthMask(GL_TRUE);
    }

    /*
     * Terrain : orthophoto réelle drapée sur le relief, avec brume au loin.
     * Si les données réelles manquent, on retombe sur le shader à couleurs de
     * sommets et le damier plat de repli.
     */
    if (m_terrain->textured()) {
        m_terrainShader->use();
        m_terrainShader->setMat4("u_view", view);
        m_terrainShader->setMat4("u_proj", proj);
        m_terrainShader->setMat4("u_model", mat4(1.0f));
        m_terrainShader->setVec3("u_lightDir", lightDir);
        m_terrainShader->setVec3("u_seaColor", SEA_COLOR);
        m_terrainShader->setVec3("u_camPos", m_camera.position());
        m_terrainShader->setVec3("u_fogColor", FOG_COLOR);
        m_terrainShader->setFloat("u_fogStart", FOG_START);
        m_terrainShader->setFloat("u_fogEnd", FOG_END);
        m_terrainShader->setInt("u_texture", 0);
        m_terrain->bindTexture(0);
        m_terrain->draw();
    } else {
        m_shader->use();
        m_shader->setMat4("u_view", view);
        m_shader->setMat4("u_proj", proj);
        m_shader->setVec3("u_lightDir", lightDir);
        m_shader->setMat4("u_model", mat4(1.0f));
        m_terrain->draw();
    }

    /*
     * Bâtiments 3D extrudés (BD TOPO) : éclairés et noyés dans la même brume que le
     * terrain pour un raccord cohérent au loin. Maillage statique unique ; rien si
     * le terrain n'en fournit pas.
     */
    if (m_buildings && m_buildings->built()) {
        m_buildingShader->use();
        m_buildingShader->setMat4("u_view", view);
        m_buildingShader->setMat4("u_proj", proj);
        m_buildingShader->setMat4("u_model", mat4(1.0f));
        m_buildingShader->setVec3("u_lightDir", lightDir);
        m_buildingShader->setVec3("u_camPos", m_camera.position());
        m_buildingShader->setVec3("u_fogColor", FOG_COLOR);
        m_buildingShader->setFloat("u_fogStart", FOG_START);
        m_buildingShader->setFloat("u_fogEnd", FOG_END);
        m_buildings->draw();
    }

    /*
     * Hélipads : celui de la zone de départ (où l'appareil démarre et où le reset,
     * touche X ou R, le ramène), plus ceux propres au terrain (hôpital, ports...)
     * listés dans helipads.txt. Chacun est posé à plat juste au-dessus du sol pour
     * rester visible sans accrocher le relief.
     */
    if (m_helipad) {
        /* Le disque du pad est quasiment dans le plan du sol : avec le test de
           profondeur, les deux se disputaient la profondeur et le pad se brisait en
           damier (z-fighting), surtout posé au ras de l'eau (Capbreton). Le pad est un
           décalque au sol, dessiné AVANT l'appareil : on désactive son test de
           profondeur, il se pose donc simplement sur ce qui est déjà rendu (terrain ou
           mer) sans jamais se disputer la profondeur. Il n'écrit pas non plus la
           profondeur ; l'appareil, dessiné après, le recouvre proprement. */
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);

        /* Pose un hélipad à plat au point (x, z) du monde, juste au-dessus du sol.
           On cale le disque sur la hauteur du sol AU CENTRE, c'est-à-dire le niveau
           où repose l'appareil : ainsi les patins touchent toujours le pad. (Sur une
           forte pente, le bord amont du disque peut affleurer le relief, moindre mal
           comparé à un pad qui flotterait au-dessus des patins.) */
        const auto drawPad = [&](float x, float z) {
            const float padTop  = m_terrain->heightAt(x, z);
            const mat4 padModel = glm::translate(mat4(1.0f), vec3{x, padTop + 0.08f, z});
            if (m_helipadModel) {
                /* Version texturée (modèle Blender), dessinée avec le shader des modèles. */
                m_modelShader->use();
                m_modelShader->setMat4("u_view", view);
                m_modelShader->setMat4("u_proj", proj);
                m_modelShader->setMat4("u_model", padModel);
                m_modelShader->setVec3("u_lightDir", lightDir);
                m_modelShader->setVec3("u_camPos", m_camera.position());
                m_modelShader->setInt("u_texture", 0);
                m_helipadModel->draw(*m_modelShader, render::Pass::Opaque);
            } else {
                /* Repli procédural (aplats de couleur). */
                m_shader->use();
                m_shader->setMat4("u_view", view);
                m_shader->setMat4("u_proj", proj);
                m_shader->setMat4("u_model", padModel);
                m_shader->setVec3("u_lightDir", lightDir);
                m_helipad->draw();
            }
        };

        /* Hélipad de départ. */
        drawPad(m_startPos.x, m_startPos.z);

        /* Hélipads du terrain, convertis de lon/lat en position monde et ignorés
           s'ils tombent hors de l'emprise courante. */
        const float halfW = m_terrain->halfWidth();
        const float halfH = m_terrain->halfHeight();
        for (const render::Landmark& pad : m_terrain->helipads()) {
            float x = 0.0f, z = 0.0f;
            m_terrain->worldAt(pad.lon, pad.lat, x, z);
            if (std::fabs(x) <= halfW && std::fabs(z) <= halfH) {
                drawPad(x, z);
            }
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }

    /*
     * Ombre portée, en deux disques de taille fixe (pas d'animation de taille) :
     *  - le fuselage : petit disque dense, toujours présent ;
     *  - le rotor : grand disque plus clair dont seule l'opacité suit le régime,
     *    invisible rotor arrêté. Ainsi l'ombre reste cohérente avec l'état des
     *    pales sans grandir ni rétrécir. Les deux s'estompent avec l'altitude.
     */
    const vec3  heliPos     = vec3(base[3]);
    const float ground      = m_terrain->heightAt(heliPos.x, heliPos.z);
    const float altitude    = heliPos.y - ground > 0.0f ? heliPos.y - ground : 0.0f;
    const float shadowAlpha = 0.26f * clamp(1.0f - altitude / 40.0f, 0.0f, 1.0f);
    if (shadowAlpha > 0.01f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        /* Posée sur un hélipad, l'ombre (juste au-dessus du sol) se retrouve très
           proche en profondeur du disque du pad : avec le test de profondeur, elle se
           dessinait en damier sur le pad (z-fighting). L'ombre est un décalque au sol
           dessiné AVANT l'appareil ; on désactive donc son test de profondeur, si bien
           qu'elle se fond simplement sur ce qui est déjà au sol (terrain ou pad) sans
           jamais se disputer la profondeur. L'appareil, dessiné après, la recouvre. */
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        const float scaleXZ = 1.0f + altitude * 0.02f;
        const float radius  = 6.0f * scaleXZ;  /* rayon du disque rotor (m), cf. disc(6, ...) */

        /*
         * Le disque est plat : sur un sol en pente, il couperait le relief et la
         * ligne d'intersection scintillerait au moindre mouvement de la caméra.
         * Le rayon (6 m) étant plus petit qu'une maille du terrain, le sol sous
         * le disque est une facette inclinée dont le point haut est un coin : on
         * pose donc le disque au-dessus du plus haut des quatre coins, avec une
         * marge, pour qu'il ne traverse jamais le sol.
         */
        /* Pose le disque juste au-dessus du plus haut des quatre coins de son
           emprise, pour qu'il ne traverse jamais un sol en pente. */
        const auto topUnder = [&](const vec3& c, float r) {
            float t = m_terrain->heightAt(c.x, c.z);
            t       = std::fmax(t, m_terrain->heightAt(c.x + r, c.z + r));
            t       = std::fmax(t, m_terrain->heightAt(c.x + r, c.z - r));
            t       = std::fmax(t, m_terrain->heightAt(c.x - r, c.z + r));
            t       = std::fmax(t, m_terrain->heightAt(c.x - r, c.z - r));
            return t;
        };

        m_shadowShader->use();
        m_shadowShader->setMat4("u_view", view);
        m_shadowShader->setMat4("u_proj", proj);

        /* Disque rotor : centré sous l'axe du mât (et non sous le centre de
           l'appareil), pour que l'ombre des pales tombe au bon endroit. Le mât est
           en avant de l'origine, le long de l'axe X du fuselage. */
        vec3 rotorCenter = heliPos;
        if (m_loadedHeli) {
            rotorCenter =
                vec3(base * vec4(render::LoadedHelicopter::ROTOR_FORWARD_OFFSET, 0.0f, 0.0f, 1.0f));
        }
        const float rotorShadowAlpha = shadowAlpha * 0.7f * clamp(rotorFraction, 0.0f, 1.0f);
        if (rotorShadowAlpha > 0.01f) {
            const vec3 rotorShadowPos{rotorCenter.x, topUnder(rotorCenter, radius) + 0.30f,
                                      rotorCenter.z};
            m_shadowShader->setMat4("u_model", glm::translate(mat4(1.0f), rotorShadowPos) *
                                                   glm::scale(mat4(1.0f), vec3{scaleXZ, 1.0f, scaleXZ}));
            m_shadowShader->setFloat("u_alpha", rotorShadowAlpha);
            m_shadowDisc->draw();
        }

        /* Fuselage : petit disque dense centré sur l'axe du rotor principal (le mât),
           comme l'ombre du rotor -- les deux disques sont ainsi concentriques sous le
           rotor. */
        const vec3  bodyCenter = rotorCenter;
        /* Disque toujours présent (même rotor arrêté), dimensionné à l'empreinte de
           l'appareil posé (~5 m) pour rester bien visible au départ, et non un petit
           disque caché sous la cabine. */
        const float bodyScale  = scaleXZ * (5.0f / 6.0f);
        const float bodyRadius = 5.0f * scaleXZ;
        const vec3  bodyShadowPos{bodyCenter.x, topUnder(bodyCenter, bodyRadius) + 0.30f,
                                  bodyCenter.z};
        m_shadowShader->setMat4("u_model", glm::translate(mat4(1.0f), bodyShadowPos) *
                                               glm::scale(mat4(1.0f), vec3{bodyScale, 1.0f, bodyScale}));
        m_shadowShader->setFloat("u_alpha", shadowAlpha);
        m_shadowDisc->draw();

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    /* Hélicoptère : modèle texturé réel s'il est chargé, sinon version dessinée. */
    if (m_loadedHeli) {
        m_modelShader->use();
        m_modelShader->setMat4("u_view", view);
        m_modelShader->setMat4("u_proj", proj);
        m_modelShader->setVec3("u_lightDir", lightDir);
        m_modelShader->setVec3("u_camPos", m_camera.position());
        m_modelShader->setInt("u_texture", 0);
        /* Assiette réelle (roulis, tangage) extraite de l'orientation rendue, pour
           animer l'horizon artificiel du tableau de bord. Axes du corps dans le
           monde : avant = colonne 0, haut = colonne 1, droite = colonne 2. */
        const vec3  fwd    = glm::normalize(vec3(base[0]));
        const vec3  upv    = glm::normalize(vec3(base[1]));
        const vec3  rgt    = glm::normalize(vec3(base[2]));
        const float pitchR = std::asin(clamp(fwd.y, -1.0f, 1.0f));
        const float rollR  = std::atan2(-rgt.y, upv.y);
        /* Altitude au-dessus du niveau de la mer (y = 0), en pieds, pour l'altimètre. */
        const float altitudeFt = base[3].y * 3.28084f;
        /* Vitesse verticale en ft/min (m/s * 196.85), pour le vario. */
        const float varioFpm = m_flight.body().velocity.y * 196.85f;
        /* Cap (radians) extrait du vecteur avant, pour le compas du tableau de bord. */
        const float headingRad = std::atan2(fwd.x, fwd.z);
        /* Vitesse air en noeuds (vitesse horizontale * 1.94384), pour l'anémomètre. */
        const float airspeedKt =
            glm::length(vec2{m_flight.body().velocity.x, m_flight.body().velocity.z}) * 1.94384f;
        /* Couple estimé en pourcentage, pour le couplemètre : le collectif commande la
           puissance, atténué par la fraction de régime rotor (0 rotor arrêté). */
        const float torquePct = collective * 100.0f * rotorFraction;

        /* En vue cockpit, le pilote est dessiné sans tête ni casque (la caméra est
           à hauteur de ses yeux) : on garde ses bras et ses jambes. Le palonnier
           fait basculer pédales et jambes. */
        m_loadedHeli->draw(*m_modelShader, base, rotorAngle, m_viewMode != 1, rudder,
                           cyclicLong, cyclicLat, collective, rollR, pitchR, altitudeFt, varioFpm,
                           headingRad, airspeedKt, torquePct);

        /*
         * Disque rotor (mis en commentaire, à reprendre plus tard).
         * À haut régime, on remplacerait les pales distinctes (estompées côté
         * modèle) par ce disque translucide dans le plan rotor, pour éviter l'effet
         * stroboscopique d'un rotor tournant à vitesse réelle :
         *
         * if (rotorFraction > 0.01f) {
         *     const vec3  hub   = m_loadedHeli->mainHubWorld(base);
         *     const float scale = render::LoadedHelicopter::MAIN_ROTOR_RADIUS / 6.0f;
         *     glEnable(GL_BLEND);
         *     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         *     glDepthMask(GL_FALSE);
         *     m_flatShader->use();
         *     m_flatShader->setMat4("u_view", view);
         *     m_flatShader->setMat4("u_proj", proj);
         *     m_flatShader->setMat4("u_model", glm::translate(mat4(1.0f), hub) *
         *                               glm::scale(mat4(1.0f), vec3{scale, 1.0f, scale}));
         *     m_flatShader->setVec4("u_color", vec4{0.18f, 0.18f, 0.20f, rotorFraction * 0.55f});
         *     m_shadowDisc->draw();
         *     glDepthMask(GL_TRUE);
         *     glDisable(GL_BLEND);
         * }
         */
    } else {
        m_helicopter->draw(*m_shader, base, rotorAngle);
    }

    /* Lueurs moteur (strombo + tuyère), dessinées en dernier car translucides. */
    drawEngineEffects(base, turbineFraction, timeSeconds);
}

void Application::drawEngineEffects(const mat4& base, float turbineFraction, float timeSeconds) {
    /* Rien tant que la turbine n'est pas lancée. */
    if (turbineFraction <= 0.01f) {
        return;
    }

    /* Position d'une lueur exprimée en repère corps (X avant, Y haut, Z droite),
       comme COCKPIT_EYE, puis ramenée dans le monde par la pose 'base'. */
    const auto bodyToWorld = [&](const vec3& bodyPos) {
        return vec3(base * vec4(bodyPos, 1.0f));
    };

    /* Une petite sphère lumineuse de couleur unie, mélangée par-dessus la scène. */
    const auto drawGlow = [&](const vec3& worldPos, float radius, const vec4& color) {
        if (color.a <= 0.01f) {
            return;
        }
        m_flatShader->setMat4("u_model", glm::translate(mat4(1.0f), worldPos) *
                                             glm::scale(mat4(1.0f), vec3{radius}));
        m_flatShader->setVec4("u_color", color);
        m_glowSphere->draw();
    };

    m_flatShader->use();
    m_flatShader->setMat4("u_view", m_camera.view());
    m_flatShader->setMat4("u_proj", m_camera.proj());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);  /* les lueurs ne masquent rien : elles s'ajoutent au rendu */

    /* --- Strombo ------------------------------------------------------------- */
    /* Petite sphère rouge au-dessus de la cabine, qui clignote : allumée seulement au
       début de chaque période, éteinte le reste du temps, tant que la turbine tourne. */
    const float beaconPhase = std::fmod(timeSeconds, BEACON_PERIOD) / BEACON_PERIOD;
    if (beaconPhase < BEACON_ON) {
        drawGlow(bodyToWorld(BEACON_POS), BEACON_RADIUS, vec4{1.0f, 0.08f, 0.08f, 0.95f});
    }

    /* --- Feux de navigation et de queue -------------------------------------- */
    /* Les feux de position (rouge bâbord, vert tribord, blanc de queue) ne servent
       qu'à la nuit. Le vol de nuit n'étant pas modélisé, ils restent éteints de jour ;
       on garde le code (et les positions) prêt pour une future ambiance nocturne. */
    constexpr bool NAV_LIGHTS_ON = false;
    if (NAV_LIGHTS_ON) {
        const auto drawLight = [&](const vec3& bodyPos, const vec3& rgb) {
            const vec3 w = bodyToWorld(bodyPos);
            drawGlow(w, NAV_RADIUS, vec4{rgb, 0.95f});
            drawGlow(w, NAV_RADIUS * 2.2f, vec4{rgb, 0.22f});
        };
        drawLight(NAV_LEFT_POS, vec3{1.0f, 0.05f, 0.05f});   /* bâbord : rouge */
        drawLight(NAV_RIGHT_POS, vec3{0.05f, 1.0f, 0.10f});  /* tribord : vert */
        drawLight(TAIL_LIGHT_POS, vec3{1.0f, 1.0f, 0.95f});  /* queue : blanc */
    }

    /* --- Tuyère -------------------------------------------------------------- */
    /* Air chaud rejeté par la turbine : pas de flamme, mais une distorsion thermique.
       On la suggère par un léger halo bleuté très translucide qui ondule doucement
       (deux sinus de fréquences différentes pour un scintillement non répétitif), plus
       un petit coeur tiède sur le métal de la tuyère. L'effet croît avec le régime. */
    const float heat    = clamp(turbineFraction, 0.0f, 1.0f);
    const float shimmer = 0.85f + 0.15f * std::sin(timeSeconds * 9.0f) * std::sin(timeSeconds * 5.3f);
    const vec3  nozzleWorld = bodyToWorld(NOZZLE_BODY_POS);

    /* Halo bleuté (air chaud) : large, presque transparent, taille qui ondule. */
    drawGlow(nozzleWorld, NOZZLE_RADIUS * shimmer, vec4{0.55f, 0.75f, 1.0f, 0.10f * heat});
    /* Coeur un peu plus dense, plus resserré, légèrement plus chaud (blanc bleuté). */
    drawGlow(nozzleWorld, NOZZLE_RADIUS * 0.5f * shimmer, vec4{0.80f, 0.88f, 1.0f, 0.14f * heat});

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Application::buildNavHud(ui::HudData& hud, const vec3& heliPos, float headingDeg) {
    if (!m_terrain || !m_terrain->hasGeo()) {
        return;
    }
    const float halfW = m_terrain->halfWidth();
    const float halfH = m_terrain->halfHeight();

    /* Minimap : orthophoto + position de l'appareil (fractions 0-1 dans l'emprise). */
    hud.mapTexId      = m_terrain->orthoTexId();
    hud.mapHeliU      = heliPos.x / (2.0f * halfW) + 0.5f;
    hud.mapHeliV      = heliPos.z / (2.0f * halfH) + 0.5f;
    /* headingDeg est déjà un cap boussole (0 = nord, 90 = est, sens horaire), ce
       qu'attend la flèche de la minimap (nord en haut). */
    hud.mapHeadingDeg = headingDeg;

    /* Étiquette 3D + point minimap d'un lieu (nom + position WGS84), s'il tombe dans
       l'emprise du terrain courant. Projetée légèrement au-dessus du sol. */
    const mat4 viewProj = m_camera.proj() * m_camera.view();
    const auto addLabel = [&](const render::Landmark& place, const char* displayName) {
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(place.lon, place.lat, x, z);
        if (std::fabs(x) > halfW || std::fabs(z) > halfH) {
            return;  /* hors du terrain courant */
        }
        ui::HudLabel label;
        label.name = displayName;
        label.mapU = x / (2.0f * halfW) + 0.5f;
        label.mapV = z / (2.0f * halfH) + 0.5f;

        const float y    = m_terrain->heightAt(x, z) + 25.0f;
        const vec4  clip = viewProj * vec4{x, y, z, 1.0f};
        if (clip.w > 0.1f) {
            const vec3 ndc = vec3(clip) / clip.w;
            if (ndc.z < 1.0f && std::fabs(ndc.x) < 1.02f && std::fabs(ndc.y) < 1.02f) {
                label.fx       = ndc.x * 0.5f + 0.5f;
                label.fy       = 1.0f - (ndc.y * 0.5f + 0.5f);
                label.depth    = clip.w;  /* distance caméra : sert à donner la priorité au plus proche */
                label.onScreen = true;
            }
        }
        hud.labels.push_back(label);
    };

    /* Lieux remarquables (étiquetés par leur nom), puis hélipads (tous étiquetés
       "Hélisurface", le terme d'aire de poser ; leur ville est déjà donnée par le
       lieu remarquable voisin). */
    for (const render::Landmark& lm : m_terrain->landmarks()) {
        addLabel(lm, lm.name.c_str());
    }
    for (const render::Landmark& pad : m_terrain->helipads()) {
        addLabel(pad, "Hélisurface");
    }
}

void Application::captureScreenshot(const std::filesystem::path& path) {
    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    if (fbw <= 0 || fbh <= 0) {
        std::fprintf(stderr, "[capture] taille de framebuffer invalide\n");
        return;
    }
    m_camera.setAspect(static_cast<float>(fbw) / static_cast<float>(fbh));

    /* Livrée Gendarmerie dans la capture si ARTOUSTE_SHOT_LIVERY est définie
       (pratique pour vérifier les marquages sans la boucle interactive). */
    if (m_loadedHeli && std::getenv("ARTOUSTE_SHOT_LIVERY") != nullptr) {
        m_loadedHeli->setGendarmerieLivery(true);
    }

    /*
     * Cadrage par défaut (vue trois quarts arrière). On peut le modifier via
     * des variables d'environnement pour observer plusieurs angles sans
     * recompiler.
     */
    float angle   = 2.4f;
    float radius  = 14.0f;
    float height  = 6.0f;
    float targetY = 1.8f;
    float agl     = 140.0f;  /* hauteur de vol au-dessus du sol pour la capture (m) */
    if (const char* e = std::getenv("ARTOUSTE_SHOT_ANGLE")) {
        angle = std::strtof(e, nullptr);
    }
    if (const char* e = std::getenv("ARTOUSTE_SHOT_RADIUS")) {
        radius = std::strtof(e, nullptr);
    }
    if (const char* e = std::getenv("ARTOUSTE_SHOT_HEIGHT")) {
        height = std::strtof(e, nullptr);
    }
    if (const char* e = std::getenv("ARTOUSTE_SHOT_TARGETY")) {
        targetY = std::strtof(e, nullptr);
    }
    if (const char* e = std::getenv("ARTOUSTE_SHOT_AGL")) {
        agl = std::strtof(e, nullptr);
    }

    /* L'appareil est placé en vol au-dessus de la côte (sa position de départ),
       de sorte que la capture montre le relief réel et la mer sous lui. Avec
       ARTOUSTE_SHOT_PARK, on le pose plutôt à sa position de parking (mât centré
       sur le H), pour vérifier ce placement. */
    vec3 shotPos{m_startPos.x, m_terrain->heightAt(m_startPos.x, m_startPos.z) + agl,
                 m_startPos.z};
    if (std::getenv("ARTOUSTE_SHOT_PARK") != nullptr) {
        shotPos = m_parkPos;
    }
    const mat4 base = glm::translate(mat4(1.0f), shotPos);

    if (std::getenv("ARTOUSTE_SHOT_COCKPIT") != nullptr) {
        m_viewMode = 1;  /* vue cockpit : pilote allégé, jambes animées */
        m_camera.setFovYDeg(70.0f);
        m_camera.setNear(0.05f);  /* petit : ne tranche pas la verrière toute proche */
        const vec3 eye = vec3(base * vec4(COCKPIT_EYE, 1.0f));
        m_camera.setLookAt(eye, eye + glm::normalize(vec3{1.0f, -0.22f, 0.0f}),
                           vec3{0.0f, 1.0f, 0.0f});
    } else {
        m_camera.setNear(0.5f);
        /* Décalages horizontaux du point visé (le repère corps est aligné sur le monde
           en capture) : pratique pour centrer la cabine, en avant de l'origine. */
        float targetX = 0.0f;
        float targetZ = 0.0f;
        if (const char* e = std::getenv("ARTOUSTE_SHOT_TARGETX")) {
            targetX = std::strtof(e, nullptr);
        }
        if (const char* e = std::getenv("ARTOUSTE_SHOT_TARGETZ")) {
            targetZ = std::strtof(e, nullptr);
        }
        m_camera.orbit(shotPos + vec3{targetX, targetY, targetZ}, radius, height, angle);
    }

    ui::HudData hud;
    hud.airspeedKt    = 92.0f;
    hud.headingDeg    = 47.0f;
    hud.altitudeM     = 35.0f;
    hud.varioFpm      = 240.0f;
    hud.varioMs       = 1.2f;
    hud.collectivePct = 55.0f;
    hud.rotorPct      = 100.0f;
    hud.rotorRpm      = 360.0f;
    hud.turbineRpm    = 33500.0f;
    hud.exhaustTempC  = 445.0f;   /* tuyère en croisière normale */
    hud.fuelLiters    = 480.0f;
    hud.turbine       = "EN RÉGIME";
    hud.assist        = std::getenv("ARTOUSTE_SHOT_ASSIST") != nullptr;  /* repère "MODE ASSISTE" */
    if (m_terrain->hasGeo()) {  /* coordonnées du point de capture */
        float lon = 0.0f, lat = 0.0f;
        m_terrain->lonLatAt(shotPos.x, shotPos.z, lon, lat);
        hud.geoValid = true;
        hud.lonDeg   = lon;
        hud.latDeg   = lat;
    }
    buildNavHud(hud, shotPos, hud.headingDeg);

    /*
     * On rend plusieurs images d'affilée : ImGui laisse ses fenêtres
     * auto-dimensionnées invisibles à leur première apparition (le temps de les
     * mesurer), puis elles se stabilisent.
     */
    /* Commandes de capture (pour vérifier les animations pédales/jambes/manche). */
    float shotRudder = 0.0f;
    float shotCyclicLong = 0.0f;
    float shotCyclicLat = 0.0f;
    float shotCollective = 0.0f;
    if (const char* e = std::getenv("ARTOUSTE_SHOT_RUDDER")) {
        shotRudder = std::strtof(e, nullptr);
    }
    if (const char* e = std::getenv("ARTOUSTE_SHOT_CYCLIC_LONG")) {
        shotCyclicLong = std::strtof(e, nullptr);
    }
    if (const char* e = std::getenv("ARTOUSTE_SHOT_CYCLIC_LAT")) {
        shotCyclicLat = std::strtof(e, nullptr);
    }
    if (const char* e = std::getenv("ARTOUSTE_SHOT_COLLECTIVE")) {
        shotCollective = std::strtof(e, nullptr);
    }
    /* Mode du HUD pour la capture : coins par défaut, ou via ARTOUSTE_SHOT_HUDMODE. */
    ui::HudMode shotHud = m_hudMode;
    if (const char* e = std::getenv("ARTOUSTE_SHOT_HUDMODE")) {
        const std::string v = e;
        shotHud = (v == "overlay") ? ui::HudMode::Overlay
                  : (v == "off")   ? ui::HudMode::Off
                                   : ui::HudMode::Corners;
    }
    for (int i = 0; i < 3; ++i) {
        /* Turbine au régime pour la capture : strombo et tuyère visibles (le temps
           0,1 s tombe dans la phase allumée du flash). */
        renderScene(base, 1.3f, 1.0f, shotRudder, shotCyclicLong, shotCyclicLat, shotCollective,
                    1.0f, 0.1f);
        m_hud.render(hud, shotHud, false);
    }
    glFinish();

    const std::size_t count =
        static_cast<std::size_t>(fbw) * static_cast<std::size_t>(fbh) * 3u;
    std::vector<unsigned char> pixels(count);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, fbw, fbh, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    stbi_flip_vertically_on_write(1);
    if (stbi_write_png(path.string().c_str(), fbw, fbh, 3, pixels.data(), fbw * 3) != 0) {
        std::printf("[capture] écrit %s (%dx%d)\n", path.string().c_str(), fbw, fbh);
    } else {
        std::fprintf(stderr, "[capture] échec d'écriture %s\n", path.string().c_str());
    }
}

void Application::toggleGendarmerieLivery() {
    if (!m_loadedHeli) {
        return;
    }
    m_gendarmerieLivery = !m_gendarmerieLivery;
    m_loadedHeli->setGendarmerieLivery(m_gendarmerieLivery);
}

void Application::resetToStart() {
    m_flight.reset(m_parkPos);
    m_input->reset();
    m_confirmReset = false;
}

void Application::startDemo() {
    /* Pad de départ et d'arrivée : là où l'appareil est garé (la démo y revient se
       poser). Point à survoler : la Dune du Pilat, repérée par son nom dans les lieux
       remarquables du terrain et convertie en coordonnées monde. Faute de la trouver,
       on vise le pad lui-même (la démo se contente alors d'un décollage et d'une pose). */
    const vec3 returnPad = m_startPos;
    vec3       dune      = returnPad;
    if (m_terrain) {
        for (const render::Landmark& lm : m_terrain->landmarks()) {
            if (lm.name.find("Pilat") != std::string::npos) {
                float x = 0.0f;
                float z = 0.0f;
                m_terrain->worldAt(lm.lon, lm.lat, x, z);
                dune = vec3{x, m_terrain->heightAt(x, z), z};
                break;
            }
        }
    }

    /* On repart d'un état propre : appareil sur le pad, turbine à froid, puis on
       lance le démarrage rapide et la chorégraphie. */
    resetToStart();
    m_viewMode = 2;  /* vue d'orbite pour le démarrage */
    m_flight.turbine().stopNow();
    m_flight.turbine().startFast();
    m_demo.start(returnPad, dune);
    m_audio.playMusic(m_musicPath);  /* musique de la démo (silencieux si le fichier est absent) */
}

void Application::toggleDemo() {
    if (m_demo.active()) {
        m_demo.stop();  /* le pilote reprend la main */
    } else {
        m_confirmDemo = true;  /* on demande confirmation avant de lancer la démo */
    }
}

void Application::onResize(int width, int height) {
    m_width  = width;
    m_height = height;
    glViewport(0, 0, width, height);
    if (height > 0) {
        m_camera.setAspect(static_cast<float>(width) / static_cast<float>(height));
    }
}

void Application::resizeCallback(GLFWwindow* window, int width, int height) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app != nullptr) {
        app->onResize(width, height);
    }
}

void Application::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action,
                              int /*mods*/) {
    if (action != GLFW_PRESS) {
        return;
    }
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    /* Pendant la démo, toute touche autre que V rend la main au pilote (la touche V
       elle-même bascule la démo plus bas). */
    if (app != nullptr && app->m_demo.active() && key != GLFW_KEY_V) {
        app->m_demo.stop();
    }

    /* Panneau de confirmation du reset affiché : seules les réponses Oui/Non sont
       prises en compte (O ou Entrée = Oui, N = Non), tout le reste est ignoré. */
    if (app != nullptr && app->m_confirmReset) {
        if (key == GLFW_KEY_O || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            app->resetToStart();
        } else if (key == GLFW_KEY_N) {
            app->m_confirmReset = false;
        }
        return;
    }

    /* Panneau de confirmation du lancement de la démo : O ou Entrée = Oui (on lance),
       N = Non (on annule), le reste est ignoré. */
    if (app != nullptr && app->m_confirmDemo) {
        if (key == GLFW_KEY_O || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            app->m_confirmDemo = false;
            app->startDemo();
        } else if (key == GLFW_KEY_N) {
            app->m_confirmDemo = false;
        }
        return;
    }

    switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_C:  /* change de vue (poursuite -> cockpit -> orbite) */
            if (app != nullptr) {
                app->m_viewMode = (app->m_viewMode + 1) % 3;
            }
            break;
        case GLFW_KEY_T:  /* démarre ou coupe la turbine */
            if (app != nullptr) {
                app->m_flight.turbine().toggle();
            }
            break;
        case GLFW_KEY_V:  /* lance ou arrête la démonstration automatique */
            if (app != nullptr) {
                app->toggleDemo();
            }
            break;
        case GLFW_KEY_L:  /* bascule la livrée Gendarmerie nationale */
            if (app != nullptr) {
                app->toggleGendarmerieLivery();
            }
            break;
        case GLFW_KEY_M:          /* bascule le mode assisté (confort de pilotage) */
        case GLFW_KEY_SEMICOLON:  /* position de la touche "M" sur un clavier AZERTY */
            if (app != nullptr) {
                app->m_assist.toggle();
            }
            break;
        case GLFW_KEY_R:  /* demande la confirmation avant de replacer l'appareil au départ */
            if (app != nullptr) {
                app->m_confirmReset = true;
            }
            break;
        case GLFW_KEY_H:  /* fait défiler les modes HUD : coins -> superposé -> rien */
            if (app != nullptr) {
                app->m_hudMode =
                    static_cast<ui::HudMode>((static_cast<int>(app->m_hudMode) + 1) % 3);
            }
            break;
        case GLFW_KEY_P:  /* met en pause ou reprend */
            if (app != nullptr) {
                app->m_paused = !app->m_paused;
            }
            break;
        default:
            break;
    }
}

int Application::run() {
    if (!initWindow()) {
        return EXIT_FAILURE;
    }
    if (!initGL()) {
        return EXIT_FAILURE;
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
        mainLoop();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur fatale : %s\n", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

}  /* namespace artouste::app */
