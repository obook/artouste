/*
 * ApplicationRender.cpp
 * Rendu d'une image : ciel, mer, terrain, bâtiments, puis l'appareil. Les
 * décalques au sol (hélipads et ombre portée) sont rendus par
 * ApplicationGround.cpp, appelé depuis renderScene. Les lueurs du moteur
 * vivent dans ApplicationRenderEffects.cpp, et l'heure/direction du soleil
 * dans ApplicationSun.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <glad/glad.h>

#include "app/AppConstants.hpp"
#include "render/Buildings.hpp"
#include "render/Vegetation.hpp"
#include "render/Clouds.hpp"
#include "render/combat/ExplosionFx.hpp"
#include "render/combat/SkinnedZombies.hpp"
#include "render/combat/Projectiles.hpp"
#include "render/Camera.hpp"
#include "render/HelicopterModel.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Mesh.hpp"
#include "render/Shader.hpp"
#include "render/Skybox.hpp"
#include "render/Terrain.hpp"
#include "render/Texture.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app {

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
    /* La lune est modélisée à l'opposé du soleil (voir drawGroundShadow) : elle est
       donc levée quand le soleil est couché. */
    m_sky->draw(*m_skyShader, glm::inverse(proj * mat4(mat3(view))), lightDir, -lightDir, timeSeconds);

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
        m_terrainShader->setInt("u_detail", 1);
        m_terrainShader->setVec2("u_originXZ",
                                 vec2{m_renderOrigin.x, m_renderOrigin.z});
        if (m_terrainDetail) {
            m_terrainDetail->bind(1);
        }
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
        m_buildingShader->setInt("u_facade", 0);
        if (m_buildingFacade) {
            m_buildingFacade->bind(0);
        }
        /* Culling par tuiles : le recalage d'origine (u_model = toRel) s'annule dans le
           produit final, donc le frustum en coordonnées monde s'extrait de proj * vue
           monde (m_camera.view()), et la caméra est prise en position monde. */
        m_buildings->draw(proj * m_camera.view(), m_camera.position());
    }

    /*
     * Végétation en billboards (arbres) : test alpha et écriture de profondeur
     * (pas de mélange), donc l'ordre de dessin importe peu. Même brume que le
     * terrain et les bâtiments pour un raccord cohérent au loin.
     */
    if (m_vegetation && m_vegetation->built()) {
        m_vegetationShader->use();
        m_vegetationShader->setMat4("u_view", view);
        m_vegetationShader->setMat4("u_proj", proj);
        m_vegetationShader->setMat4("u_model", toRel);
        m_vegetationShader->setVec3("u_lightDir", lightDir);
        m_vegetationShader->setVec3("u_camPos", camPosRel);
        m_vegetationShader->setVec3("u_fogColor", fogColor);
        m_vegetationShader->setFloat("u_fogStart", FOG_START);
        m_vegetationShader->setFloat("u_fogEnd", FOG_END);
        m_vegetationShader->setInt("u_texture", 0);
        /* Alpha-to-coverage : le bord du feuillage est tramé sur les sous-échantillons
           (MSAA déjà actif), pour des contours doux plutôt qu'un seuil net. Sans
           mélange (le shader écrit une profondeur), donc l'ordre de dessin importe peu. */
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        m_vegetation->draw();
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }

    /*
     * Nuages en billboards : couche de cumulus au-dessus du relief. Transparence par
     * mélange alpha, avec tri arrière -> avant fait dans Clouds::draw ; sans écriture
     * de profondeur (le relief devant les masque, ils ne s'occultent pas entre eux).
     */
    if (m_clouds && m_clouds->built()) {
        m_cloudShader->use();
        m_cloudShader->setMat4("u_view", view);
        m_cloudShader->setMat4("u_proj", proj);
        m_cloudShader->setMat4("u_model", toRel);
        m_cloudShader->setVec3("u_lightDir", lightDir);
        m_cloudShader->setVec3("u_camPos", camPosRel);
        m_cloudShader->setVec3("u_fogColor", fogColor);
        m_cloudShader->setFloat("u_fogStart", FOG_START);
        m_cloudShader->setFloat("u_fogEnd", FOG_END);
        m_cloudShader->setInt("u_texture", 0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        m_clouds->draw(m_camera.position());
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    /*
     * Mode zombie : pack de personnages SKINNES (marche + bras animés), rendu
     * instancié par groupes de phase (voir render::SkinnedZombies). Mêmes
     * uniformes d'éclairage/brume que le modèle normal, plus u_bones (matrices
     * d'os) posé par le renderer par lot. Le tampon d'instances est reconstruit
     * chaque image à partir de l'état courant de la horde (CombatMode) :
     * position/orientation de chaque zombie, et son "kind" (variante + groupe).
     * timeSeconds sert d'horloge d'animation.
     */
    if (m_combat.active() && m_zombiesRender && m_zombiesRender->built()) {
        m_zombieShader->use();
        m_zombieShader->setMat4("u_model", toRel);
        m_zombieShader->setMat4("u_view", view);
        m_zombieShader->setMat4("u_proj", proj);
        m_zombieShader->setVec3("u_lightDir", lightDir);
        m_zombieShader->setVec3("u_camPos", camPosRel);
        m_zombieShader->setVec3("u_fogColor", fogColor);
        m_zombieShader->setFloat("u_fogStart", FOG_START);
        m_zombieShader->setFloat("u_fogEnd", FOG_END);
        m_zombieShader->setInt("u_texture", 0);
        m_zombiesRender->updateInstances(m_combat.zombieTransforms(), m_combat.zombieHitFlashes(),
                                         m_combat.zombieKinds());
        m_zombiesRender->draw(*m_zombieShader, timeSeconds);
    }

    /*
     * Boulettes toxiques : billboard face caméra, mêmes uniformes que les
     * zombies (pas de texture, la forme est procédurale, voir
     * projectile.frag).
     */
    if (m_combat.active() && m_projectilesRender && m_projectilesRender->built()) {
        m_projectileShader->use();
        m_projectileShader->setMat4("u_model", toRel);
        m_projectileShader->setMat4("u_view", view);
        m_projectileShader->setMat4("u_proj", proj);
        m_projectilesRender->updateInstances(m_combat.projectileInstances());
        m_projectilesRender->draw();
    }


    /*
     * Explosions 3D des roquettes : modèle animé émissif joué a chaque impact
     * (voir render::ExplosionFx). Rendu additif dans la passe principale (profondeur
     * lue, pas écrite) pour s'occulter correctement derriere le relief et l'appareil.
     */
    if (m_combat.active() && m_explosionFx && m_explosionFx->built()) {
        const auto blasts = m_combat.explosions();
        if (!blasts.empty()) {
            std::vector<render::ExplosionFx::Instance> instances;
            instances.reserve(blasts.size());
            for (const auto& b : blasts) {
                instances.push_back(render::ExplosionFx::Instance{b.center, b.progress});
            }
            m_explosionShader->use();
            m_explosionShader->setMat4("u_view", view);
            m_explosionShader->setMat4("u_proj", proj);
            m_explosionShader->setInt("u_texture", 0);
            m_explosionFx->draw(*m_explosionShader, toRel, instances);
        }
    }


    /* Décalques au sol, dessinés avant l'appareil (voir ApplicationGround.cpp). */
    drawHelipads(view, proj, lightDir);
    drawHapi(view, proj, timeSeconds);
    drawScorchMarks(view, proj);  /* traces d'impact des roquettes (mode zombie) */
    drawGroundShadow(base, rotorFraction, view, proj, lightDir);

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

}  /* namespace artouste::app */
