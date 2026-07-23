/*
 * ApplicationGround.cpp
 * Décalques posés au sol et dessinés avant l'appareil : les hélipads (départ et
 * ceux du terrain) et les traces de brûlure laissées par les impacts de
 * roquettes. Le rendu de l'ombre portée de l'appareil vit dans
 * ApplicationGroundShadow.cpp et celui des balises HAPI dans
 * ApplicationGroundHapi.cpp. Les hélipads gardent le test de profondeur (le
 * relief peut donc les cacher) mais tirent leur profondeur vers la caméra
 * (polygon offset) pour éviter le z-fighting au ras du relief ; ils n'écrivent
 * pas la profondeur et l'appareil, dessiné après, les recouvre.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "render/Camera.hpp"
#include "render/Mesh.hpp"
#include "render/Model.hpp"
#include "render/Shader.hpp"
#include "render/Terrain.hpp"
#include "util/Math.hpp"

#include <glad/glad.h>

#include <cmath>

namespace artouste::app {

void Application::drawHelipads(const mat4& view, const mat4& proj, const vec3& lightDir) {
    /*
     * Hélipads : celui de la zone de départ (où l'appareil démarre et où le reset,
     * touche X ou R, le ramène), plus ceux propres au terrain (hôpital, ports...)
     * listés dans helipads.txt. Chacun est posé à plat juste au-dessus du sol pour
     * rester visible sans accrocher le relief.
     */
    if (!m_helipad) {
        return;
    }

    /* Le disque du pad est quasiment dans le plan du sol : au test de profondeur
       brut, les deux se disputaient la profondeur et le pad se brisait en damier
       (z-fighting), surtout posé au ras de l'eau (Capbreton). Couper le test
       réglait le damier mais faisait du pad un tampon : ses pixels recouvraient
       tout le paysage déjà rendu, montagnes comprises (flagrant au sommet du pic
       du Midi d'Ossau). On garde donc le test et on tire la profondeur du pad
       vers la caméra (polygon offset) : il gagne contre le sol qui le porte, mais
       le relief devant lui le cache normalement. Il n'écrit pas la profondeur ;
       l'appareil, dessiné après, le recouvre proprement. */
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);

    /* Pose un hélipad à plat au point (x, z) du monde, juste au-dessus du sol.
       On cale le disque sur la hauteur du sol AU CENTRE, c'est-à-dire le niveau
       où repose l'appareil : ainsi les patins touchent toujours le pad. (Sur une
       forte pente, le bord amont du disque peut affleurer le relief, moindre mal
       comparé à un pad qui flotterait au-dessus des patins.) */
    /* Rendu relatif à la caméra : la vue reçue est déjà relative à m_renderOrigin ;
       on retranche donc la même origine des positions monde et de u_camPos. */
    const vec3 camPosRel = m_camera.position() - m_renderOrigin;
    const auto drawPad = [&](float x, float z) {
        const float padTop = m_terrain->heightAt(x, z);
        const mat4 padModel = glm::translate(
            mat4(1.0f), vec3{x - m_renderOrigin.x, padTop + 0.08f, z - m_renderOrigin.z});
        /* Jupe sous le disque : sur un pad perché, le plateau surplombe le relief ;
           la paroi cylindrique habille la tranche (sa partie enterrée est cachée
           par le test de profondeur). Vraie géométrie : elle écrit la profondeur,
           pour que le disque, dessiné après sans l'écrire, ne transparaisse pas au
           travers vu de dessous. */
        if (m_padSkirt) {
            glDepthMask(GL_TRUE);
            m_shader->use();
            m_shader->setMat4("u_view", view);
            m_shader->setMat4("u_proj", proj);
            m_shader->setMat4(
                "u_model",
                glm::translate(mat4(1.0f),
                               vec3{x - m_renderOrigin.x, padTop + 0.06f, z - m_renderOrigin.z}));
            m_shader->setVec3("u_lightDir", lightDir);
            m_padSkirt->draw();
            glDepthMask(GL_FALSE);
        }
        if (m_helipadModel) {
            /* Version texturée (modèle Blender), dessinée avec le shader des modèles. */
            m_modelShader->use();
            m_modelShader->setMat4("u_view", view);
            m_modelShader->setMat4("u_proj", proj);
            m_modelShader->setMat4("u_model", padModel);
            m_modelShader->setVec3("u_lightDir", lightDir);
            m_modelShader->setVec3("u_camPos", camPosRel);
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
    const float origX = m_terrain->originX();
    const float origZ = m_terrain->originZ();
    for (const render::Landmark& pad : m_terrain->helipads()) {
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(pad.lon, pad.lat, x, z);
        if (std::fabs(x - origX) <= halfW && std::fabs(z - origZ) <= halfH) {
            drawPad(x, z);
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);
    glDepthMask(GL_TRUE);
}

void Application::drawScorchMarks(const mat4& view, const mat4& proj) {
    if (!m_combat.active() || !m_shadowShader || !m_shadowDisc) {
        return;
    }
    const std::vector<RocketSystem::ScorchView> scorches = m_combat.scorches();
    if (scorches.empty()) {
        return;
    }

    constexpr float DISC_MESH_R = 6.0f;   /* rayon du maillage de disque (cf. disc(6, ...)) */
    constexpr float SCORCH_R_M = 3.5f;    /* rayon au sol de la trace (m) */
    constexpr float SCORCH_ALPHA = 0.55f; /* opacité max (gris foncé, pas noir pur) */
    const float scale = SCORCH_R_M / DISC_MESH_R;

    m_shadowShader->use();
    m_shadowShader->setMat4("u_view", view);
    m_shadowShader->setMat4("u_proj", proj);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    /* Décalage de profondeur pour ne pas lutter avec le sol (comme l'ombre). */
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);

    for (const RocketSystem::ScorchView& s : scorches) {
        const mat4 model =
            glm::translate(mat4(1.0f),
                           vec3{s.center.x, s.center.y + 0.05f, s.center.z} - m_renderOrigin) *
            glm::scale(mat4(1.0f), vec3{scale, 1.0f, scale});
        m_shadowShader->setMat4("u_model", model);
        m_shadowShader->setFloat("u_alpha", SCORCH_ALPHA * s.alpha);
        m_shadowDisc->draw();
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} /* namespace artouste::app */
