/*
 * ApplicationLifecycle.cpp
 * Cycle de vie du simulateur : ouverture de la fenêtre, chargement d'OpenGL,
 * point d'entrée run() et destruction ordonnée des ressources. La gestion de
 * la fenêtre au runtime (plein écran, redimensionnement) vit dans
 * ApplicationWindow.cpp, et les actions de session (livrée, reset, démo) dans
 * ApplicationSession.cpp. La mise en place de la scène, la boucle principale,
 * le rendu, les entrées, le HUD et la capture d'écran sont répartis dans les
 * fichiers ApplicationScene/Loop/Render/Ground/Input/Hud/Capture.cpp, qui
 * partagent tous la même classe (Application.hpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <glad/glad.h>
/* glad doit précéder GLFW. */
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include "input/Gamepad.hpp"
#include "input/InputSystem.hpp"
#include "render/Buildings.hpp"
#include "render/Vegetation.hpp"
#include "render/Clouds.hpp"
#include "render/HelicopterModel.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Mesh.hpp"
#include "render/Model.hpp"
#include "render/Shader.hpp"
#include "render/Skybox.hpp"
#include "render/Terrain.hpp"
#include "render/Texture.hpp"
#include "render/combat/ExplosionFx.hpp"
#include "render/combat/SkinnedZombies.hpp"
#include "render/combat/Projectiles.hpp"
#include "render/combat/ZombieEyes.hpp"

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

/* Charge un PNG en RGBA pour l'icône de fenêtre. Contrairement aux textures
   OpenGL (voir Texture.cpp), GLFW attend l'origine en haut à gauche : pas de
   retournement vertical ici. */
bool chargerIconeFenetre(const std::filesystem::path& path, GLFWimage& image) {
    stbi_set_flip_vertically_on_load(0);
    int            width    = 0;
    int            height   = 0;
    int            channels = 0;
    unsigned char* pixels =
        stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        return false;
    }
    image.width  = width;
    image.height = height;
    image.pixels = pixels;
    return true;
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
    m_vegetation.reset();
    m_clouds.reset();
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
    m_vegetationShader.reset();
    m_cloudShader.reset();
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

    /* Base de mappings manette (SDL_GameControllerDB) : reconnaît les manettes
       absentes de la base intégrée de GLFW, dont les Xbox récentes en Bluetooth. */
    input::Gamepad::loadMappings(resolveAssetDir());

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    /* Anti-crénelage (MSAA) : clé "msaa" de config.txt (4x par défaut), surchargée par
       la variable d'environnement ARTOUSTE_MSAA (prioritaire). Le MSAA coûte cher en
       bande passante sur GPU intégré, d'où ce levier (0, 2, 4, 8). */
    int samples = m_config.msaa;
    if (const char* env = std::getenv("ARTOUSTE_MSAA"); env != nullptr && env[0] != '\0') {
        samples = std::atoi(env);
    }
    glfwWindowHint(GLFW_SAMPLES, samples);
    /* Sans ça, GLFW minimise automatiquement la fenêtre plein écran dès qu'elle perd le
       focus (ex. activer une fenêtre sur un autre écran en configuration multi-moniteur). */
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

    m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (m_window == nullptr) {
        std::fprintf(stderr, "Création de la fenêtre GLFW échouée.\n");
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);  /* synchronisation verticale (vsync) */

    /* Icône de la fenêtre (barre de titre, barre des tâches Windows). Plusieurs
       tailles : GLFW choisit celle qui convient au contexte d'affichage. */
    const std::filesystem::path   assets = resolveAssetDir();
    std::vector<GLFWimage>        icones;
    for (const int taille : {16, 32, 48, 128, 256}) {
        GLFWimage image{};
        if (chargerIconeFenetre(assets / "icons" / ("icon-" + std::to_string(taille) + ".png"),
                                 image)) {
            icones.push_back(image);
        }
    }
    if (!icones.empty()) {
        glfwSetWindowIcon(m_window, static_cast<int>(icones.size()), icones.data());
        for (const GLFWimage& image : icones) {
            stbi_image_free(image.pixels);
        }
    }

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
    std::printf("Framebuffer : %d x %d\n", m_width, m_height);
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    return true;
}

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

    /* Plein écran sans bordure au lancement, sauf en mode capture (qui garde son
       1280x720 pour un cadrage reproductible) ou si ARTOUSTE_WINDOWED force le fenêtré
       (pratique en développement). La touche F bascule ensuite à volonté. */
    if (std::getenv("ARTOUSTE_SCREENSHOT") == nullptr &&
        std::getenv("ARTOUSTE_WINDOWED") == nullptr) {
        setFullscreen(true);
    }

    /* Menu de démarrage dans la fenêtre (remplace launch.bat, bloqué par le Contrôle
       intelligent des applications de Windows). Sauté en mode capture, quand la carte
       est déjà imposée par une variable d'environnement (scripts, tests), ou sur
       demande explicite (ARTOUSTE_NO_MENU). */
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
                             std::getenv("ARTOUSTE_NO_MENU") == nullptr;
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
           dans le menu -- comme la fermeture de la fenêtre -- termine l'application. */
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

}  /* namespace artouste::app */
