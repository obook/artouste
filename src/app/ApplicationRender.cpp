/*
 * ApplicationRender.cpp
 * Rendu d'une image : ciel, mer, terrain, bâtiments, puis l'appareil et les
 * lueurs du moteur. Les décalques au sol (hélipads et ombre portée) sont rendus
 * par ApplicationGround.cpp, appelé depuis renderScene.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <glad/glad.h>

#include "app/AppConstants.hpp"
#include "render/Buildings.hpp"
#include "render/Camera.hpp"
#include "render/HelicopterModel.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Mesh.hpp"
#include "render/Shader.hpp"
#include "render/Skybox.hpp"
#include "render/Terrain.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app {

namespace {

/*
 * Repères des effets moteur, dans le même repère corps que COCKPIT_EYE (X avant,
 * Y haut, Z droite, origine au centre de l'appareil). Réglés à l'oeil via le mode
 * capture (ARTOUSTE_SHOT_*).
 *
 * Strombo (feu anti-collision) : petite sphère rouge posée au-dessus de la cabine,
 * qui clignote (allumée une brève fraction de la période) tant que la turbine tourne.
 */
const vec3      BEACON_POS{3.11f, 2.41f, 0.0f};  /* position du beacon du modèle (all-lights.xml), sur le toit */
constexpr float BEACON_RADIUS = 0.07f;           /* rayon de la sphère (m) */
constexpr float BEACON_PERIOD = 1.2f;            /* période du clignotement (s) */
constexpr float BEACON_ON     = 0.18f;           /* fraction de la période où le feu est allumé */

/*
 * Feux de position avant, aux positions du modèle (all-lights.xml d'Émmanuel),
 * exprimées ici en repère corps. Allumés la nuit (voir renderScene). On respecte la
 * convention aéronautique : rouge à bâbord (gauche), vert à tribord (droite). Le feu
 * blanc de queue du modèle (corps {-3.96, 2.13, 0.20}) n'est pas allumé pour l'instant.
 */
const vec3      NAV_LEFT_POS{4.24f, 0.92f, -0.77f};   /* feu de navigation bâbord (rouge) */
const vec3      NAV_RIGHT_POS{4.24f, 0.92f, 0.77f};   /* feu de navigation tribord (vert) */
constexpr float NAV_RADIUS = 0.05f;                   /* rayon du coeur d'un feu (m) */

/*
 * Tuyère : sortie de la turbine, derrière le bloc moteur, au départ de la poutre.
 * La turbine est haute sur le pont moteur (derrière le mât) : la sortie est donc en
 * arrière de l'origine (X négatif) et en hauteur, pas dans le carter (plus bas, plus
 * en avant). On ne dessine pas de flamme : la turbine rejette de l'air très chaud,
 * qui se traduit par une distorsion thermique localisée (léger halo bleuté qui ondule)
 * et un petit coeur tiède sur le métal de la tuyère.
 */
const vec3      NOZZLE_BODY_POS{-0.30f, 2.28f, 0.0f};
constexpr float NOZZLE_RADIUS = 0.24f;

}  /* namespace */

float Application::timeOfDaySeconds(float t) const {
    /* On part de l'heure locale du PC au lancement (m_sunBaseSeconds), puis on
       avance à m_sunTimeScale fois le temps réel (1 = temps réel, 0 = figé). */
    constexpr float DAY = 86400.0f;  /* secondes dans une journée */
    float secondsOfDay = std::fmod(m_sunBaseSeconds + t * m_sunTimeScale, DAY);
    if (secondsOfDay < 0.0f) {
        secondsOfDay += DAY;  /* échelle négative : on reste dans [0, 86400[ */
    }
    return secondsOfDay;
}

vec3 Application::sunDirection(float t) const {
    /* Course du soleil : midi -> zénith (y max), 6 h / 18 h -> horizon, minuit ->
       sous l'horizon. Le -pi/2 cale midi (43200 s) sur le zénith. Le repère monde a
       X vers l'est et Z vers le sud (voir ApplicationHud) : le grand axe est-ouest
       est donc porté par X (lever a l'est, coucher a l'ouest), et le décalage fixe en
       Z (0.35) incline l'arc vers le sud, comme dans l'hémisphère nord. */
    constexpr float DAY = 86400.0f;
    const float angle = TWO_PI * (timeOfDaySeconds(t) / DAY) - HALF_PI;
    return glm::normalize(vec3{std::cos(angle), std::sin(angle), 0.35f});
}

void Application::renderScene(const mat4& base, float rotorAngle, float rotorFraction,
                             float rudder, float cyclicLong, float cyclicLat,
                             float collective, float turbineFraction, float timeSeconds) {
    const vec3 lightDir = sunDirection(timeSeconds);
    const mat4 proj     = m_camera.proj();

    /* Rendu relatif à la caméra : on retranche la position horizontale de la caméra
       (X, Z seulement, pour ne pas décaler les altitudes) de la vue et de toutes les
       géométries. Le GPU ne manipule alors que de petites coordonnées près de la
       caméra, ce qui supprime le tremblement de précision en grandes coordonnées
       monde. Mathématiquement équivalent : vue_rel * translate(-origine) * modèle =
       vue * modèle. La position caméra relative sert au brouillard et à l'éclairage. */
    m_renderOrigin       = vec3{m_camera.position().x, 0.0f, m_camera.position().z};
    const mat4 toRel     = glm::translate(mat4(1.0f), -m_renderOrigin);
    const mat4 view      = m_camera.view() * glm::translate(mat4(1.0f), m_renderOrigin);
    const vec3 camPosRel = m_camera.position() - m_renderOrigin;

    /* Couleur de fond assombrie la nuit (le ciel plein écran la recouvre, mais le
       tampon de profondeur, lui, est bien remis à zéro). */
    const float isDay = glm::clamp(lightDir.y + 0.2f, 0.0f, 1.0f);
    glClearColor(0.53f * isDay, 0.70f * isDay, 0.92f * isDay, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Brume du lointain : on l'assombrit la nuit (sinon la mer et l'horizon se fondent
       dans une brume claire sous un ciel sombre, ce qui les fait paraître blancs). On
       garde un léger fond bleuté nocturne (facteur plancher) accordé au ciel de nuit. */
    const vec3 fogColor = FOG_COLOR * glm::mix(0.06f, 1.0f, isDay);

    /* Ciel en dégradé (il remplit le fond de l'image). On passe l'inverse de
       (projection * rotation caméra seule) : en retirant la translation (position
       caméra, en milliers de mètres), le ciel reconstruit la direction du rayon sans
       soustraction de grands nombres, ce qui supprime le tremblement du soleil. */
    m_sky->draw(*m_skyShader, glm::inverse(proj * mat4(mat3(view))), lightDir);

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
        m_seaShader->setMat4("u_model", toRel);
        m_seaShader->setVec3("u_seaColor", SEA_COLOR);
        m_seaShader->setVec3("u_lightDir", lightDir);
        m_seaShader->setVec3("u_camPos", camPosRel);
        m_seaShader->setVec3("u_fogColor", fogColor);
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
        m_terrainShader->setMat4("u_model", toRel);
        m_terrainShader->setVec3("u_lightDir", lightDir);
        m_terrainShader->setVec3("u_seaColor", SEA_COLOR);
        m_terrainShader->setVec3("u_camPos", camPosRel);
        m_terrainShader->setVec3("u_fogColor", fogColor);
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
        m_shader->setMat4("u_model", toRel);
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
        m_buildingShader->setMat4("u_model", toRel);
        m_buildingShader->setVec3("u_lightDir", lightDir);
        m_buildingShader->setVec3("u_camPos", camPosRel);
        m_buildingShader->setVec3("u_fogColor", fogColor);
        m_buildingShader->setFloat("u_fogStart", FOG_START);
        m_buildingShader->setFloat("u_fogEnd", FOG_END);
        m_buildings->draw();
    }

    /* Décalques au sol, dessinés avant l'appareil (voir ApplicationGround.cpp). */
    drawHelipads(view, proj, lightDir);
    drawGroundShadow(base, rotorFraction, view, proj);

    /* Hélicoptère : modèle texturé réel s'il est chargé, sinon version dessinée. */
    if (m_loadedHeli) {
        m_modelShader->use();
        m_modelShader->setMat4("u_view", view);
        m_modelShader->setMat4("u_proj", proj);
        m_modelShader->setVec3("u_lightDir", lightDir);
        m_modelShader->setVec3("u_camPos", camPosRel);
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
        /* La pose monde 'base' sert aux calculs d'instruments ci-dessus (assiette,
           altitude, cap) ; pour le DESSIN, on passe la pose relative à l'origine de
           rendu (toRel * base), cohérente avec la vue et les autres géométries. */
        m_loadedHeli->draw(*m_modelShader, toRel * base, rotorAngle, m_viewMode != 1, rudder,
                           cyclicLong, cyclicLat, collective, rollR, pitchR, altitudeFt, varioFpm,
                           headingRad, airspeedKt, torquePct);

        /* (Un disque rotor translucide remplaçant les pales distinctes à haut régime,
           pour éviter l'effet stroboscopique, reste à étudier ; voir l'historique
           git pour une ébauche.) */
    } else {
        m_helicopter->draw(*m_shader, toRel * base, rotorAngle);
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

    /* Une petite sphère lumineuse de couleur unie, mélangée par-dessus la scène.
       Rendu relatif à la caméra : on retranche m_renderOrigin de la position monde,
       en accord avec la vue relative posée plus bas. */
    const auto drawGlow = [&](const vec3& worldPos, float radius, const vec4& color) {
        if (color.a <= 0.01f) {
            return;
        }
        m_flatShader->setMat4("u_model", glm::translate(mat4(1.0f), worldPos - m_renderOrigin) *
                                             glm::scale(mat4(1.0f), vec3{radius}));
        m_flatShader->setVec4("u_color", color);
        m_glowSphere->draw();
    };

    m_flatShader->use();
    m_flatShader->setMat4("u_view", m_camera.view() * glm::translate(mat4(1.0f), m_renderOrigin));
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

    /* --- Feux de position avant (nuit) --------------------------------------- */
    /* Les deux feux de nez (rouge bâbord, vert tribord) s'allument la nuit, entre 18h
       et 6h dans l'heure du simulateur (du coucher au lever du soleil). De jour ils
       restent éteints. */
    const float hourOfDay = timeOfDaySeconds(timeSeconds) / 3600.0f;
    if (hourOfDay >= 18.0f || hourOfDay < 6.0f) {
        const auto drawLight = [&](const vec3& bodyPos, const vec3& rgb) {
            const vec3 w = bodyToWorld(bodyPos);
            drawGlow(w, NAV_RADIUS, vec4{rgb, 0.95f});
            drawGlow(w, NAV_RADIUS * 2.2f, vec4{rgb, 0.22f});
        };
        drawLight(NAV_LEFT_POS, vec3{1.0f, 0.05f, 0.05f});   /* bâbord : rouge */
        drawLight(NAV_RIGHT_POS, vec3{0.05f, 1.0f, 0.10f});  /* tribord : vert */
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

}  /* namespace artouste::app */
