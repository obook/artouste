/*
 * ApplicationCapture.cpp
 * Capture d'écran hors boucle interactive : déclenchée par la variable
 * d'environnement ARTOUSTE_SCREENSHOT, elle place l'appareil et la caméra selon
 * d'autres variables ARTOUSTE_SHOT_*, rend quelques images puis enregistre un PNG.
 * Pratique pour vérifier le rendu et les marquages sans piloter.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "app/AppConstants.hpp"
#include "render/Camera.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"
#include "util/Math.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace artouste::app {

void Application::captureScreenshot(const std::filesystem::path& path) {
    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    if (fbw <= 0 || fbh <= 0) {
        std::fprintf(stderr, "[capture] taille de framebuffer invalide\n");
        return;
    }
    m_camera.setAspect(static_cast<float>(fbw) / static_cast<float>(fbh));

    /* Livrée de la capture : ARTOUSTE_SHOT_LIVERY=0 force la livrée d'origine,
       toute autre valeur force la Gendarmerie. Variable absente : on garde l'état
       par défaut (Gendarmerie). Pratique pour vérifier les deux livrées. */
    if (m_loadedHeli) {
        if (const char* e = std::getenv("ARTOUSTE_SHOT_LIVERY")) {
            m_loadedHeli->setGendarmerieLivery(e[0] != '0');
        }
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

}  /* namespace artouste::app */
