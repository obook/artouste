/*
 * Application.cpp
 * Cycle de vie du simulateur : ouverture de la fenêtre, chargement d'OpenGL,
 * point d'entrée run() et destruction ordonnée des ressources. La mise en place
 * de la scène, la boucle principale, le rendu, les entrées, le HUD et la capture
 * d'écran sont répartis dans les fichiers ApplicationScene/Loop/Render/Ground/
 * Input/Hud/Capture.cpp, qui partagent tous la même classe (Application.hpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <glad/glad.h>
/* glad doit précéder GLFW. */
#include <GLFW/glfw3.h>

#include "input/InputSystem.hpp"
#include "render/Buildings.hpp"
#include "render/HelicopterModel.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Mesh.hpp"
#include "render/Model.hpp"
#include "render/Shader.hpp"
#include "render/Skybox.hpp"
#include "render/Terrain.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

namespace artouste::app {

namespace {

constexpr int  WINDOW_WIDTH   = 1280;
constexpr int  WINDOW_HEIGHT  = 720;
constexpr char WINDOW_TITLE[] = "Artouste";

void glfwErrorCallback(int code, const char* description) {
    std::fprintf(stderr, "[GLFW] erreur %d : %s\n", code, description);
}

/* Manette branchée ou débranchée à chaud : on transmet l'évènement à Gamepad,
 * qui remet le levier de collectif à zéro à la déconnexion. */
void joystickCallback(int jid, int event) {
    input::Gamepad::onJoystickEvent(jid, event);
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
    glfwSetJoystickCallback(joystickCallback);
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
    /* La démo se déroule sur le bassin d'Arcachon (survol du cap Ferret puis d'Arcachon).
       Si une autre carte est affichée (choix du menu), on bascule d'abord sur Arcachon :
       sinon la démo se jouerait sur la carte courante, sans ces lieux à survoler. */
    if (m_terrainName != "arcachon") {
        std::printf("[démo] terrain forcé sur arcachon pour la démonstration.\n");
        loadTerrain("arcachon");
    }

    /* Pad de départ et d'arrivée : là où l'appareil est garé (la démo y revient se
       poser). */
    const vec3 returnPad = m_startPos;

    /* Route de la démo (voir ROADMAP.md, section Mode demo) : Dune du Pilat à 2000 m
       (passage haut, panorama), puis cap Ferret par son nord en rase-mottes à 30 m, puis
       Arcachon à 1000 m, avant de revenir se poser au pad. Sans terrain, la route reste
       vide : la démo se contente alors d'un décollage suivi d'une pose. */
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
        /* Même chose, mais à partir d'un lieu remarquable du terrain repéré par son nom
           (rien n'est ajouté si le lieu est absent). */
        auto ajouterLieu = [&](const char* nom, float altitude) {
            for (const render::Landmark& lm : m_terrain->landmarks()) {
                if (lm.name.find(nom) != std::string::npos) {
                    ajouter(lm.lon, lm.lat, altitude);
                    return;
                }
            }
        };
        /* Dune du Pilat et cap Ferret nord : coordonnées explicites du point de survol.
           Arcachon : repéré par son nom dans les lieux remarquables. */
        ajouter(-1.2075204f, 44.5846722f, 2000.0f);  /* Dune du Pilat (passage haut) */
        ajouter(-1.2582492f, 44.6634685f, 30.0f);    /* cap Ferret par son nord (rase-mottes) */
        ajouterLieu("Arcachon", 1000.0f);            /* Arcachon */
    }

    /* On repart d'un état propre : appareil sur le pad, turbine à froid, puis on
       lance le démarrage rapide et la chorégraphie. */
    resetToStart();
    m_viewMode = 2;  /* vue d'orbite pour le démarrage */
    m_flight.turbine().stopNow();
    m_flight.turbine().startFast();
    m_demoUserView = false;  /* la démo reprend la main sur la vue et le HUD */
    m_demoUserHud  = false;
    m_demo.start(returnPad, route);
    /* Musique de la démo mise de côté pour l'instant : on ne la lance pas.
       (Tout le mécanisme reste en place ; décommenter pour la réactiver.) */
    /* m_audio.playMusic(m_musicPath); */
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
