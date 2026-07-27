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
#include "physics/constants.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"
#include "util/Math.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
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

    /* Livrée de la capture : ARTOUSTE_SHOT_LIVERY=0 force la blanche, =2 l'armée de
       terre, =3 la Protection civile, toute autre valeur la Gendarmerie. Variable
       absente : on garde l'état par défaut (Gendarmerie). */
    if (m_loadedHeli) {
        if (const char* e = std::getenv("ARTOUSTE_SHOT_LIVERY")) {
            render::Livery liv = render::Livery::Gendarmerie;
            if (e[0] == '0') {
                liv = render::Livery::Blanche;
            } else if (e[0] == '2') {
                liv = render::Livery::ArmeeDeTerre;
            } else if (e[0] == '3') {
                liv = render::Livery::ProtectionCivile;
            }
            m_loadedHeli->setLivery(liv);
        }
    }

    /* Heure du soleil pour la capture : ARTOUSTE_SHOT_HOUR fixe l'heure du jour (0 à
       24) afin d'obtenir un lever, un plein jour ou un coucher de soleil reproductible
       (par exemple 17.7 pour un soleil bas sur la mer). On fige le temps (échelle 0)
       pour que le rendu reste à cette heure. */
    if (const char* e = std::getenv("ARTOUSTE_SHOT_HOUR")) {
        m_sunBaseSeconds = std::strtof(e, nullptr) * 3600.0f;
        m_sunTimeScale   = 0.0f;
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
       de sorte que la capture montre le relief réel et la mer sous lui.
       ARTOUSTE_SHOT_LON / ARTOUSTE_SHOT_LAT le placent ailleurs (terrain
       géoréférencé), toujours à agl mètres au-dessus du sol (0 = posé) :
       pratique pour cadrer un lieu précis (sommet, dune...). Avec
       ARTOUSTE_SHOT_PARK, on le pose plutôt à sa position de parking (mât centré
       sur le H), pour vérifier ce placement. */
    float shotX = m_startPos.x;
    float shotZ = m_startPos.z;
    const char* shotLon = std::getenv("ARTOUSTE_SHOT_LON");
    const char* shotLat = std::getenv("ARTOUSTE_SHOT_LAT");
    if (shotLon != nullptr && shotLat != nullptr && m_terrain->hasGeo()) {
        m_terrain->worldAt(std::strtof(shotLon, nullptr), std::strtof(shotLat, nullptr),
                           shotX, shotZ);
    }
    vec3 shotPos{shotX, m_terrain->heightAt(shotX, shotZ) + agl, shotZ};
    if (std::getenv("ARTOUSTE_SHOT_PARK") != nullptr) {
        shotPos = m_parkPos;
    }
    /* Cap de l'appareil : ARTOUSTE_SHOT_HEADING (cap boussole en degrés, 0 = nord,
       90 = est). Par défaut le nez pointe vers l'est (+X monde), comme au départ.
       Permet une vue arrière orientée vers un paysage choisi. */
    mat4 base = glm::translate(mat4(1.0f), shotPos);
    if (const char* e = std::getenv("ARTOUSTE_SHOT_HEADING")) {
        const float headingDeg = std::strtof(e, nullptr);
        base = base * glm::rotate(mat4(1.0f), glm::radians(90.0f - headingDeg),
                                  vec3{0.0f, 1.0f, 0.0f});
    }

    if (std::getenv("ARTOUSTE_SHOT_COCKPIT") != nullptr) {
        m_viewMode = 1;  /* vue cockpit : pilote allégé, jambes animées */
        m_camera.setFovYDeg(70.0f);
        m_camera.setNear(0.05f);  /* petit : ne tranche pas la verrière toute proche */
        const vec3 eye = vec3(base * vec4(COCKPIT_EYE, 1.0f));
        m_camera.setLookAt(eye, eye + glm::normalize(vec3{1.0f, -0.22f, 0.0f}),
                           vec3{0.0f, 1.0f, 0.0f});
    } else if (const char* solar = std::getenv("ARTOUSTE_SHOT_SOLAR")) {
        /* Vue d'orbite solaire (plan "golden hour") : on synthétise un soleil bas
           dont la hauteur vaut ARTOUSTE_SHOT_SOLAR (0,2 par défaut) et on cadre
           comme en jeu, pour vérifier que l'appareil reste dans le cadre. */
        m_viewMode = 3;
        m_camera.setFovYDeg(60.0f);
        m_camera.setNear(0.5f);
        float elev = std::strtof(solar, nullptr);
        if (elev <= 0.0f) {
            elev = 0.2f;
        }
        const vec3 sunDir = glm::normalize(vec3{0.35f, elev, 1.0f});
        m_camera.orbitSolar(shotPos + vec3{0.0f, targetY, 0.0f}, sunDir, radius, height);
    } else {
        /* Caméra placée en orbite autour de l'appareil : il faut donc le dessiner
           en entier. Le vol commence en cockpit, où le modèle est allégé (voir
           ApplicationRenderActors) ; sans ce retour à la vue extérieure, la
           capture cadrerait un appareil incomplet. */
        m_viewMode = 0;
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
    /* Vitesse de croisière affichée : ~170 km/h, mais plafonnée à 90 % de la VNE
       du moment (elle décroît avec l'altitude), pour que la LED IAS reste verte
       même sur une capture en altitude : sinon 170 km/h dépasse la VNE au-delà de
       ~2000 m (par ex. ~164 km/h à 2800 m) et le voyant passe au rouge. Réglable
       par ARTOUSTE_SHOT_IAS. */
    const float vneKmh = physics::vneAtAltitudeMs(shotPos.y) * 3.6f;
    const float cruise = 0.90f * vneKmh;
    hud.airspeedKmh   = (cruise < 170.0f) ? cruise : 170.0f;
    if (const char* e = std::getenv("ARTOUSTE_SHOT_IAS")) {
        hud.airspeedKmh = std::strtof(e, nullptr);
    }
    hud.headingDeg    = 47.0f;
    hud.altitudeM     = shotPos.y;  /* vraie altitude du point de capture */
    hud.varioMs       = 1.2f;
    hud.collectivePct = 55.0f;
    hud.rotorPct      = 100.0f;
    hud.rotorRpm      = 360.0f;
    hud.rotorLedArmed = true;     /* rotor au régime : LED NR verte sur la capture */
    hud.turbineRpm    = 33500.0f;
    hud.exhaustTempC  = 445.0f;   /* tuyère en croisière normale */
    hud.fuelLiters    = 480.0f;
    hud.turbine       = "EN RÉGIME";
    if (std::getenv("ARTOUSTE_SHOT_TURBINE_OFF") != nullptr) {
        /* Turbine à l'arrêt : tous les voyants doivent s'éteindre. */
        hud.rotorRpm      = 0.0f;
        hud.rotorLedArmed = false;
        hud.turbineRpm    = 0.0f;
        hud.turbine       = "ARRÊT";
    }
    hud.assist        = std::getenv("ARTOUSTE_SHOT_ASSIST") != nullptr;  /* repère "MODE ASSISTÉ" */
    if (const char* e = std::getenv("ARTOUSTE_SHOT_VORTEX")) {
        hud.vrsIntensity = std::strtof(e, nullptr);  /* force le bandeau d'alerte vortex */
    }
    hud.sinkRateAlert = std::getenv("ARTOUSTE_SHOT_SINKRATE") != nullptr;  /* bandeau taux de descente */
    if (m_terrain->hasGeo()) {  /* coordonnées du point de capture */
        float lon = 0.0f, lat = 0.0f;
        m_terrain->lonLatAt(shotPos.x, shotPos.z, lon, lat);
        hud.geoValid = true;
        hud.lonDeg   = lon;
        hud.latDeg   = lat;
    }
    buildNavHud(hud, shotPos, hud.headingDeg, 0.0f);  /* capture déterministe : phase "allumée" */

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
    /* Angle du rotor (rad) : figé par défaut pour une capture reproductible, réglable
       par ARTOUSTE_SHOT_ROTORANGLE (utile pour enchaîner des captures avec des pales
       à des angles différents, par exemple pour composer une séquence animée). */
    float shotRotorAngle = 1.3f;
    if (const char* e = std::getenv("ARTOUSTE_SHOT_ROTORANGLE")) {
        shotRotorAngle = std::strtof(e, nullptr);
    }
    /* Tuiles de détail : en vol elles arrivent au fil des images, mais une
       capture n'en rend que trois. On laisse donc la fenêtre se remplir avant de
       photographier, sinon la carte serait immortalisée floue -- exactement ce
       qu'on cherche à vérifier. Plafond de temps pour ne jamais bloquer : une
       carte sans tuiles, ou un disque absent, ne doit pas empêcher la capture.
       Un dt généreux fait aussi terminer les fondus d'un coup. */
    if (m_terrain->detail() != nullptr) {
        const render::tuiles::Fenetre* large = m_terrain->detail();
        const render::tuiles::Fenetre* serree = m_terrain->detailFin();
        for (int i = 0; i < 400; ++i) {
            /* Le suivi d'abord : c'est lui qui recense les tuiles attendues, et
               le compte partirait de zéro si on le testait avant. */
            m_terrain->suivreDetail(m_camera.position().x, m_camera.position().z, 1.0f);
            if (large->stabilisee() && (serree == nullptr || serree->stabilisee())) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::printf("[capture] tuiles de détail : %d / %d en place",
                    large->residentes(),
                    large->attendues());
        if (serree != nullptr) {
            std::printf(", niveau serré %d / %d", serree->residentes(), serree->attendues());
        }
        std::printf(".\n");
    }

    for (int i = 0; i < 3; ++i) {
        /* Turbine au régime pour la capture : strombo et tuyère visibles (le temps
           0,1 s tombe dans la phase allumée du flash). */
        renderScene(base, shotRotorAngle, 1.0f, shotRudder, shotCyclicLong, shotCyclicLat,
                    shotCollective, 1.0f, 0.1f);
        m_hud.updateScale(fbw, fbh);  /* échelle et police, avant le NewFrame ImGui */
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
