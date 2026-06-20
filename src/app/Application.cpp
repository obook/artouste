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

#include "app/Application.hpp"

#include <glad/glad.h>
/* glad doit précéder GLFW. */
#include <GLFW/glfw3.h>

#include "app/Clock.hpp"
#include "input/InputSystem.hpp"
#include "physics/RigidBody.hpp"
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
std::filesystem::path resolveAssetDir() {
    namespace fs = std::filesystem;
    if (const char* env = std::getenv("ARTOUSTE_ASSETS")) {
        if (fs::exists(env)) {
            return fs::path(env);
        }
    }
    if (fs::exists(ARTOUSTE_ASSET_DIR)) {
        return fs::path(ARTOUSTE_ASSET_DIR);
    }
    std::error_code  ec;
    const fs::path   exe = fs::canonical("/proc/self/exe", ec);
    if (!ec) {
        const fs::path local = exe.parent_path() / "assets";
        if (fs::exists(local)) {
            return local;
        }
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
    m_terrain.reset();
    m_sea.reset();
    m_shadowDisc.reset();
    m_glowSphere.reset();
    m_helipad.reset();
    m_helipadModel.reset();
    m_sky.reset();
    m_flatShader.reset();
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
    m_sky         = std::make_unique<render::Skybox>();

    const auto discData = render::primitives::disc(6.0f, 48, vec3{0.0f, 0.0f, 0.0f});
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

    /* Terrain réel de la côte basque (relief IGN + orthophoto drapée). */
    m_terrain = std::make_unique<render::Terrain>(assets / "terrain");

    /*
     * Position de départ : posé sur la côte de Hendaye, l'océan à l'ouest et le
     * relief (jusqu'à La Rhune) vers l'est. Si le terrain réel est absent, on
     * reste à l'origine sur le sol plat de repli.
     */
    if (m_terrain->textured()) {
        constexpr float START_X = -9140.0f;  /* m à l'est du centre (négatif = ouest) */
        constexpr float START_Z = 3006.0f;   /* m au sud du centre */
        const float     ground  = m_terrain->heightAt(START_X, START_Z);
        m_startPos              = vec3{START_X, ground, START_Z};
        /* L'appareil se gare mât rotor centré sur le H : son origine (que la
           physique place) est donc reculée de ROTOR_FORWARD_OFFSET le long de l'axe
           de départ (cap initial = +X, orientation identité au reset). */
        const float parkX = START_X - render::LoadedHelicopter::ROTOR_FORWARD_OFFSET;
        m_parkPos         = vec3{parkX, m_terrain->heightAt(parkX, START_Z), START_Z};
        m_flight.reset(m_parkPos);
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
        /* Commandes brutes du pilote, puis passage par le mode assisté : s'il est
         * actif, il corrige et adoucit les commandes ; sinon il les laisse intactes. */
        const physics::Controls controls = m_assist.apply(m_input->poll(frameDt), frameDt);

        /* Croix directionnelle haut de la manette : bascule le mode assisté (comme la touche M). */
        if (m_input->assistTogglePressed()) {
            m_assist.toggle();
        }

        /* Bouton Y de la manette : change de vue, comme la touche C du clavier. */
        if (m_input->viewTogglePressed()) {
            m_viewMode = (m_viewMode + 1) % 3;
        }

        /* Bouton Start de la manette : démarre ou coupe la turbine, comme la touche T. */
        if (m_input->turbineTogglePressed()) {
            m_flight.turbine().toggle();
        }

        /* Boutons manette équivalents aux touches clavier H, P, R et Échap, pour
         * pouvoir jouer à la manette seule. */
        if (m_input->hudTogglePressed()) {  /* B : fait défiler les modes HUD (comme H) */
            m_hudMode = static_cast<ui::HudMode>((static_cast<int>(m_hudMode) + 1) % 3);
        }
        if (m_input->pauseTogglePressed()) {  /* Back : pause/reprise (comme P) */
            m_paused = !m_paused;
        }
        if (m_input->resetPressed()) {  /* X : replace l'appareil au départ (comme R) */
            m_flight.reset(m_parkPos);
            m_input->reset();
        }
        if (m_input->quitPressed()) {  /* LB + RB : quitte (comme Échap) */
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
        if (m_input->liveryTogglePressed()) {  /* A : bascule la livrée (comme L) */
            toggleGendarmerieLivery();
        }

        /* Hauteur du relief sous l'appareil : sert au contact avec le sol. */
        const vec3& pos = m_flight.body().position;
        m_flight.setGroundHeight(m_terrain->heightAt(pos.x, pos.z));

        /* État physique avant le dernier pas, conservé pour interpoler le rendu. */
        physics::RigidBody prevBody = m_flight.body();
        if (m_paused) {
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

        /* Caméra : poursuite, cockpit (solidaire de l'appareil) ou orbite libre. */
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
            m_camera.orbit(lookTarget, 15.0f, 6.0f, t * 0.25f);
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
        float headingDeg = glm::degrees(yaw);
        if (headingDeg < 0.0f) {
            headingDeg += 360.0f;
        }
        hud.headingDeg    = headingDeg;
        hud.varioFpm      = body.velocity.y * 196.85f;
        hud.varioMs       = body.velocity.y;
        hud.collectivePct = controls.collective * 100.0f;
        hud.pedals        = controls.pedals;
        hud.rotorPct      = rotorFraction * 100.0f;          /* régime rotor, en pourcentage */
        hud.rotorRpm      = rotorFraction * 360.0f;          /* régime rotor nominal : 360 tr/min */
        hud.turbineRpm    = turbineFraction * 33500.0f;      /* régime turbine nominal : ~33 500 tr/min */
        hud.fuelLiters    = m_flight.fuelLiters();
        hud.turbine       = m_flight.turbine().label();
        hud.assist        = m_assist.active();
        m_hud.render(hud, m_hudMode, m_paused);

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
     * Hélipad de la zone de départ : posé à plat au point de départ, juste au-dessus
     * du sol pour rester visible sans accrocher le relief. C'est là que l'appareil
     * démarre et que le reset (touche X ou R) le ramène.
     */
    if (m_helipad) {
        /* Le sol de la côte est en pente : on pose le disque juste au-dessus du
           plus haut point du relief sous l'emprise du pad, pour qu'il ne soit
           pas à moitié enfoui côté amont. */
        constexpr float padRadius = 7.0f;
        float           padTop    = m_startPos.y;
        for (const vec2 d : {vec2{padRadius, 0.0f}, vec2{-padRadius, 0.0f}, vec2{0.0f, padRadius},
                             vec2{0.0f, -padRadius}}) {
            padTop = std::fmax(padTop, m_terrain->heightAt(m_startPos.x + d.x, m_startPos.z + d.y));
        }
        const mat4 padModel =
            glm::translate(mat4(1.0f), vec3{m_startPos.x, padTop + 0.06f, m_startPos.z});

        /* Le pad est quasiment dans le plan du terrain : sans précaution, les deux
           surfaces se disputent la profondeur et scintillent (z-fighting). Le décalage
           de polygones biaise la profondeur du pad vers l'avant pour qu'il l'emporte
           toujours proprement sur le sol, quelle que soit la distance. */
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
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
        glPolygonOffset(0.0f, 0.0f);
        glDisable(GL_POLYGON_OFFSET_FILL);
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
    const float shadowAlpha = 0.35f * clamp(1.0f - altitude / 40.0f, 0.0f, 1.0f);
    if (shadowAlpha > 0.01f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
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

        m_flatShader->use();
        m_flatShader->setMat4("u_view", view);
        m_flatShader->setMat4("u_proj", proj);

        /* Disque rotor : centré sous l'axe du mât (et non sous le centre de
           l'appareil), pour que l'ombre des pales tombe au bon endroit. Le mât est
           en avant de l'origine, le long de l'axe X du fuselage. */
        vec3 rotorCenter = heliPos;
        if (m_loadedHeli) {
            rotorCenter =
                vec3(base * vec4(render::LoadedHelicopter::ROTOR_FORWARD_OFFSET, 0.0f, 0.0f, 1.0f));
        }
        const float rotorShadowAlpha = shadowAlpha * 0.55f * clamp(rotorFraction, 0.0f, 1.0f);
        if (rotorShadowAlpha > 0.01f) {
            const vec3 rotorShadowPos{rotorCenter.x, topUnder(rotorCenter, radius) + 0.30f,
                                      rotorCenter.z};
            m_flatShader->setMat4("u_model", glm::translate(mat4(1.0f), rotorShadowPos) *
                                                 glm::scale(mat4(1.0f), vec3{scaleXZ, 1.0f, scaleXZ}));
            m_flatShader->setVec4("u_color", vec4{0.0f, 0.0f, 0.0f, rotorShadowAlpha});
            m_shadowDisc->draw();
        }

        /* Fuselage : petit disque dense, toujours présent, sous le centre de l'appareil. */
        const float bodyScale  = scaleXZ * (2.8f / 6.0f);  /* empreinte fuselage ~2,8 m */
        const float bodyRadius = 2.8f * scaleXZ;
        const vec3  bodyShadowPos{heliPos.x, topUnder(heliPos, bodyRadius) + 0.30f, heliPos.z};
        m_flatShader->setMat4("u_model", glm::translate(mat4(1.0f), bodyShadowPos) *
                                             glm::scale(mat4(1.0f), vec3{bodyScale, 1.0f, bodyScale}));
        m_flatShader->setVec4("u_color", vec4{0.0f, 0.0f, 0.0f, shadowAlpha});
        m_shadowDisc->draw();

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
        /* En vue cockpit, le pilote est dessiné sans tête ni casque (la caméra est
           à hauteur de ses yeux) : on garde ses bras et ses jambes. Le palonnier
           fait basculer pédales et jambes. */
        m_loadedHeli->draw(*m_modelShader, base, rotorAngle, m_viewMode != 1, rudder,
                           cyclicLong, cyclicLat, collective);

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
    hud.fuelLiters    = 480.0f;
    hud.turbine       = "EN RÉGIME";
    hud.assist        = std::getenv("ARTOUSTE_SHOT_ASSIST") != nullptr;  /* repère "MODE ASSISTE" */

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
    for (int i = 0; i < 3; ++i) {
        /* Turbine au régime pour la capture : strombo et tuyère visibles (le temps
           0,1 s tombe dans la phase allumée du flash). */
        renderScene(base, 1.3f, 1.0f, shotRudder, shotCyclicLong, shotCyclicLat, shotCollective,
                    1.0f, 0.1f);
        m_hud.render(hud, m_hudMode, false);
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
        case GLFW_KEY_L:  /* bascule la livrée Gendarmerie nationale */
            if (app != nullptr) {
                app->toggleGendarmerieLivery();
            }
            break;
        case GLFW_KEY_M:  /* bascule le mode assisté (confort de pilotage) */
            if (app != nullptr) {
                app->m_assist.toggle();
            }
            break;
        case GLFW_KEY_R:  /* replace l'appareil à sa position de départ (la côte) */
            if (app != nullptr) {
                app->m_flight.reset(app->m_parkPos);
                app->m_input->reset();
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
