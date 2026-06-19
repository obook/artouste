#include "app/Application.hpp"

#include <glad/glad.h>
// glad doit précéder GLFW.
#include <GLFW/glfw3.h>

#include "app/Clock.hpp"
#include "input/InputSystem.hpp"
#include "physics/RigidBody.hpp"
#include "render/HelicopterModel.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Mesh.hpp"
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
#include <vector>

#ifndef ARTOUSTE_ASSET_DIR
#define ARTOUSTE_ASSET_DIR "assets"
#endif

namespace artouste::app {

namespace {

constexpr int  WINDOW_WIDTH  = 1280;
constexpr int  WINDOW_HEIGHT = 720;
constexpr char WINDOW_TITLE[] = "Artouste";

// Position de l'oeil du pilote (siège droit) dans le repère corps : avant (X),
// haut (Y), droite (Z). Réglée pour cadrer la planche de bord et la verrière.
const vec3 COCKPIT_EYE{3.40f, 1.86f, 0.42f};

void glfwErrorCallback(int code, const char* description) {
    std::fprintf(stderr, "[GLFW] erreur %d : %s\n", code, description);
}

// Localise le dossier des ressources : variable d'environnement, puis chemin
// connu à la compilation (développement), puis dossier "assets" à côté de
// l'exécutable (build packagé).
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

}  // namespace

Application::Application() = default;

Application::~Application() {
    // Les ressources GL (Shader, Mesh) doivent être détruites tant que le
    // contexte existe encore : on les libère avant de fermer GLFW.
    m_hud.shutdown();
    m_input.reset();
    m_loadedHeli.reset();
    m_helicopter.reset();
    m_terrain.reset();
    m_shadowDisc.reset();
    m_sky.reset();
    m_flatShader.reset();
    m_skyShader.reset();
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
    glfwSwapInterval(1);  // vsync

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
    const std::filesystem::path assets = resolveAssetDir();
    m_shader = std::make_unique<render::Shader>(assets / "shaders" / "basic.vert",
                                                assets / "shaders" / "basic.frag");
    m_modelShader = std::make_unique<render::Shader>(assets / "shaders" / "model.vert",
                                                     assets / "shaders" / "model.frag");
    m_skyShader   = std::make_unique<render::Shader>(assets / "shaders" / "sky.vert",
                                                     assets / "shaders" / "sky.frag");
    m_flatShader  = std::make_unique<render::Shader>(assets / "shaders" / "flat.vert",
                                                     assets / "shaders" / "flat.frag");
    m_sky         = std::make_unique<render::Skybox>();

    const auto discData = render::primitives::disc(6.0f, 48, vec3{0.0f, 0.0f, 0.0f});
    m_shadowDisc        = std::make_unique<render::Mesh>(discData.vertices, discData.indices);

    m_terrain    = std::make_unique<render::Terrain>(400.0f, 80);
    m_helicopter = std::make_unique<render::HelicopterModel>();
    m_input      = std::make_unique<input::InputSystem>(m_window);
    m_hud.init(m_window);
    m_audio.init(assets / "models" / "Alouette-II" / "Sounds");

    // Modèle FlightGear réel s'il est présent (gardé hors dépôt) ; sinon on
    // garde le placeholder procédural.
    const std::filesystem::path modelsDir = assets / "models" / "Alouette-II" / "Models";
    if (std::filesystem::exists(modelsDir / "alouette.ac")) {
        auto loaded = std::make_unique<render::LoadedHelicopter>(modelsDir);
        if (loaded->loaded()) {
            m_loadedHeli = std::move(loaded);
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
    constexpr float SIM_DT = 1.0f / 240.0f;  // pas fixe de simulation (240 Hz)

    Clock clock;
    float accumulator = 0.0f;

    while (glfwWindowShouldClose(m_window) == GLFW_FALSE) {
        glfwPollEvents();
        clock.tick();
        const float t = static_cast<float>(clock.elapsed());
        // Pas de temps borné : évite un saut à la première image ou après un
        // gel (débogage, fenêtre déplacée).
        const float frameDt = clamp(static_cast<float>(clock.dt()), 0.0f, 0.1f);

        // Entrées -> commandes, puis intégration physique à pas fixe, découplée
        // de la cadence de rendu.
        const physics::Controls controls = m_input->poll(frameDt);
        if (m_paused) {
            accumulator = 0.0f;  // pas de rattrapage à la reprise
        } else {
            accumulator += frameDt;
            while (accumulator >= SIM_DT) {
                m_flight.update(controls, SIM_DT);
                accumulator -= SIM_DT;
            }
        }

        const physics::RigidBody& body = m_flight.body();

        // Transformation monde de l'appareil : translation + orientation (quat).
        const mat4 base =
            glm::translate(mat4(1.0f), body.position) * glm::mat4_cast(body.orientation);

        // Cap (lacet) extrait de l'orientation, pour la caméra chase.
        const vec3  forward = body.orientation * vec3{1.0f, 0.0f, 0.0f};
        const float yaw     = std::atan2(-forward.z, forward.x);

        // Caméra : chase, cockpit (solidaire) ou orbite libre.
        const vec3 lookTarget = body.position + vec3{0.0f, 1.2f, 0.0f};
        if (m_viewMode == 1) {  // cockpit
            const vec3 eye = vec3(base * vec4(COCKPIT_EYE, 1.0f));
            const vec3 fwd = mat3(base) * glm::normalize(vec3{1.0f, -0.22f, 0.0f});
            const vec3 up  = mat3(base) * vec3{0.0f, 1.0f, 0.0f};
            m_camera.setFovYDeg(70.0f);
            m_camera.setLookAt(eye, eye + fwd, up);
        } else if (m_viewMode == 2) {  // orbite
            m_camera.setFovYDeg(60.0f);
            m_camera.orbit(lookTarget, 15.0f, 6.0f, t * 0.25f);
        } else {  // chase
            m_camera.setFovYDeg(60.0f);
            m_camera.chase(lookTarget, yaw, frameDt);
        }

        const float airspeed = glm::length(vec2{body.velocity.x, body.velocity.z});
        m_audio.update(controls.collective, airspeed);

        renderScene(base, t);

        ui::HudData hud;
        hud.altitudeM    = body.position.y;
        hud.airspeedKt   = airspeed * 1.94384f;
        float headingDeg = glm::degrees(yaw);
        if (headingDeg < 0.0f) {
            headingDeg += 360.0f;
        }
        hud.headingDeg    = headingDeg;
        hud.varioFpm      = body.velocity.y * 196.85f;
        hud.collectivePct = controls.collective * 100.0f;
        hud.pedals        = controls.pedals;
        hud.rotorPct      = 100.0f;  // régime supposé constant (régulateur)
        m_hud.render(hud, m_showHud, m_paused);

        glfwSwapBuffers(m_window);
    }
}

void Application::renderScene(const mat4& base, float timeSeconds) {
    const vec3 lightDir = glm::normalize(vec3{0.4f, 1.0f, 0.35f});
    const mat4 view     = m_camera.view();
    const mat4 proj     = m_camera.proj();

    glClearColor(0.53f, 0.70f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Ciel en dégradé (remplit la couleur de fond).
    m_sky->draw(*m_skyShader, glm::inverse(proj * view), m_camera.position());

    // Terrain : shader à couleur de sommet.
    m_shader->use();
    m_shader->setMat4("u_view", view);
    m_shader->setMat4("u_proj", proj);
    m_shader->setVec3("u_lightDir", lightDir);
    m_shader->setMat4("u_model", mat4(1.0f));
    m_terrain->draw();

    // Ombre portée : disque sombre translucide au sol sous l'appareil, qui
    // s'estompe et s'élargit avec l'altitude.
    const vec3  heliPos     = vec3(base[3]);
    const float altitude    = heliPos.y > 0.0f ? heliPos.y : 0.0f;
    const float shadowAlpha = 0.35f * clamp(1.0f - altitude / 40.0f, 0.0f, 1.0f);
    if (shadowAlpha > 0.01f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        const float scaleXZ = 1.0f + altitude * 0.02f;
        const mat4  shadowModel =
            glm::translate(mat4(1.0f), vec3{heliPos.x, 0.03f, heliPos.z}) *
            glm::scale(mat4(1.0f), vec3{scaleXZ, 1.0f, scaleXZ});
        m_flatShader->use();
        m_flatShader->setMat4("u_view", view);
        m_flatShader->setMat4("u_proj", proj);
        m_flatShader->setMat4("u_model", shadowModel);
        m_flatShader->setVec4("u_color", vec4{0.0f, 0.0f, 0.0f, shadowAlpha});
        m_shadowDisc->draw();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    // Hélicoptère : modèle texturé réel s'il est chargé, sinon procédural.
    if (m_loadedHeli) {
        m_modelShader->use();
        m_modelShader->setMat4("u_view", view);
        m_modelShader->setMat4("u_proj", proj);
        m_modelShader->setVec3("u_lightDir", lightDir);
        m_modelShader->setInt("u_texture", 0);
        m_loadedHeli->draw(*m_modelShader, base, timeSeconds);
    } else {
        m_helicopter->draw(*m_shader, base, timeSeconds);
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

    // Cadrage par défaut (vue 3/4 arrière), surchargeable par variables
    // d'environnement pour inspecter sous plusieurs angles sans recompiler.
    float angle  = 2.4f;
    float radius = 14.0f;
    float height = 6.0f;
    float targetY = 1.8f;
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

    if (std::getenv("ARTOUSTE_SHOT_COCKPIT") != nullptr) {
        m_camera.setFovYDeg(70.0f);
        m_camera.setLookAt(COCKPIT_EYE, COCKPIT_EYE + glm::normalize(vec3{1.0f, -0.22f, 0.0f}),
                           vec3{0.0f, 1.0f, 0.0f});
    } else {
        m_camera.orbit(vec3{0.0f, targetY, 0.0f}, radius, height, angle);
    }
    const mat4 base = mat4(1.0f);

    ui::HudData hud;
    hud.airspeedKt    = 92.0f;
    hud.headingDeg    = 47.0f;
    hud.altitudeM     = 35.0f;
    hud.varioFpm      = 240.0f;
    hud.collectivePct = 55.0f;
    hud.rotorPct      = 100.0f;

    // Plusieurs images : ImGui rend ses fenêtres auto-dimensionnées invisibles
    // à leur première apparition (mesure), elles se stabilisent ensuite.
    for (int i = 0; i < 3; ++i) {
        renderScene(base, 1.3f);
        m_hud.render(hud, m_showHud, false);
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
        case GLFW_KEY_C:  // cycle de vue (chase -> cockpit -> orbite)
            if (app != nullptr) {
                app->m_viewMode = (app->m_viewMode + 1) % 3;
            }
            break;
        case GLFW_KEY_R:  // reset position de l'appareil
            if (app != nullptr) {
                app->m_flight.reset();
                app->m_input->reset();
            }
            break;
        case GLFW_KEY_H:  // HUD on/off
            if (app != nullptr) {
                app->m_showHud = !app->m_showHud;
            }
            break;
        case GLFW_KEY_P:  // pause
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
        // Mode capture : si ARTOUSTE_SCREENSHOT pointe un chemin, on rend une
        // image, on l'enregistre et on quitte (utile pour valider le rendu).
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

}  // namespace artouste::app
