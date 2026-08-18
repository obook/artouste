/*
 * ApplicationRender.cpp
 * Orchestration du rendu d'une image : calcule les grandeurs communes
 * (matrices relatives à la caméra, brume) puis enchaîne les étapes de dessin.
 * Le ciel et la mer sont dans ApplicationRenderWorld.cpp ; le terrain, les
 * bâtiments, la végétation et les nuages dans ApplicationRenderTerrain.cpp ;
 * les entités dynamiques (mode zombie, hélicoptère) dans
 * ApplicationRenderActors.cpp ; les décalques au sol dans
 * ApplicationGround.cpp ; les lueurs du moteur dans
 * ApplicationRenderEffects.cpp ; le souffle rotor dans
 * ApplicationRenderSouffle.cpp ; l'heure/direction du soleil dans
 * ApplicationSun.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/AppConstants.hpp"
#include "app/Application.hpp"
#include "render/Camera.hpp"
#include "util/Math.hpp"

#include <glad/glad.h>

namespace artouste::app {

void Application::renderScene(const mat4& base,
                              float rotorAngle,
                              float rotorFraction,
                              float rudder,
                              float cyclicLong,
                              float cyclicLat,
                              float collective,
                              float turbineFraction,
                              float timeSeconds) {
    const vec3 lightDir = sunDirection(timeSeconds);
    const mat4 proj = m_camera.proj();

    /* Rendu relatif à la caméra : on retranche la position horizontale de la caméra
       (X, Z seulement, pour ne pas décaler les altitudes) de la vue et de toutes les
       géométries. Le GPU ne manipule alors que de petites coordonnées près de la
       caméra, ce qui supprime le tremblement de précision en grandes coordonnées
       monde. Mathématiquement équivalent : vue_rel * translate(-origine) * modèle =
       vue * modèle. La position caméra relative sert au brouillard et à l'éclairage. */
    m_renderOrigin = vec3{m_camera.position().x, 0.0f, m_camera.position().z};
    const mat4 toRel = glm::translate(mat4(1.0f), -m_renderOrigin);
    const mat4 view = m_camera.view() * glm::translate(mat4(1.0f), m_renderOrigin);
    const vec3 camPosRel = m_camera.position() - m_renderOrigin;

    /* Couleur de fond assombrie la nuit (le ciel plein écran la recouvre, mais le
       tampon de profondeur, lui, est bien remis à zéro). */
    const float isDay = glm::clamp(lightDir.y + 0.2f, 0.0f, 1.0f);
    /* ARTOUSTE_DEBUG_POISON : fond magenta et pas de ciel, pour qu'un pixel non
       dessiné se dénonce. Outil de mise au point, à retirer. */
    static const bool poison = std::getenv("ARTOUSTE_DEBUG_POISON") != nullptr;
    if (poison) {
        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    } else {
        glClearColor(0.53f * isDay, 0.70f * isDay, 0.92f * isDay, 1.0f);
    }
    /* Pochoir remis à zéro : la fenêtre de relief fin y marque son emprise pour
       que le maillage d'ensemble n'y soit pas dessiné (voir
       renderTerrainAndBuildings). */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    /* Brume du lointain : on l'assombrit la nuit (sinon la mer et l'horizon se fondent
       dans une brume claire sous un ciel sombre, ce qui les fait paraître blancs). On
       garde un léger fond bleuté nocturne (facteur plancher) accordé au ciel de nuit. */
    const vec3 fogColor = FOG_COLOR * glm::mix(0.06f, 1.0f, isDay);

    const RenderContext ctx{lightDir, proj, view, toRel, camPosRel, fogColor};

    if (!poison) {
        renderSkyAndSea(ctx, timeSeconds);
    }
    renderTerrainAndBuildings(ctx);
    renderMonuments(ctx);
    renderVegetationAndClouds(ctx);
    renderCombatEntities(ctx, timeSeconds);

    /* Décalques au sol, dessinés avant l'appareil (voir ApplicationGround.cpp). */
    drawHelipads(view, proj, lightDir);
    drawHapi(view, proj, timeSeconds);
    drawScorchMarks(view, proj); /* traces d'impact des roquettes (mode zombie) */
    drawGroundShadow(base, rotorFraction, view, proj, lightDir);

    renderHelicopter(
        ctx, base, rotorAngle, rudder, cyclicLong, cyclicLat, collective, rotorFraction);

    /* Souffle rotor : la poussière passe devant les patins et le bas du fuselage,
       elle vient donc après l'appareil. */
    drawSouffle(ctx);

    /* Lueurs moteur (strombo + tuyère), dessinées en dernier car translucides. */
    drawEngineEffects(base, turbineFraction, timeSeconds);
}

} /* namespace artouste::app */
