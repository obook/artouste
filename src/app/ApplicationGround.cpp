/*
 * ApplicationGround.cpp
 * Décalques posés au sol et dessinés avant l'appareil : les hélipads (départ et
 * ceux du terrain) et l'ombre portée de l'appareil. Tous deux sont rendus sans
 * test de profondeur pour éviter le z-fighting au ras du relief ; l'appareil,
 * dessiné après, les recouvre proprement.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <glad/glad.h>

#include "render/Camera.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Mesh.hpp"
#include "render/Model.hpp"
#include "render/Shader.hpp"
#include "render/Terrain.hpp"
#include "util/Math.hpp"

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

    /* Le disque du pad est quasiment dans le plan du sol : avec le test de
       profondeur, les deux se disputaient la profondeur et le pad se brisait en
       damier (z-fighting), surtout posé au ras de l'eau (Capbreton). Le pad est un
       décalque au sol, dessiné AVANT l'appareil : on désactive son test de
       profondeur, il se pose donc simplement sur ce qui est déjà rendu (terrain ou
       mer) sans jamais se disputer la profondeur. Il n'écrit pas non plus la
       profondeur ; l'appareil, dessiné après, le recouvre proprement. */
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    /* Pose un hélipad à plat au point (x, z) du monde, juste au-dessus du sol.
       On cale le disque sur la hauteur du sol AU CENTRE, c'est-à-dire le niveau
       où repose l'appareil : ainsi les patins touchent toujours le pad. (Sur une
       forte pente, le bord amont du disque peut affleurer le relief, moindre mal
       comparé à un pad qui flotterait au-dessus des patins.) */
    /* Rendu relatif à la caméra : la vue reçue est déjà relative à m_renderOrigin ;
       on retranche donc la même origine des positions monde et de u_camPos. */
    const vec3 camPosRel = m_camera.position() - m_renderOrigin;
    const auto drawPad = [&](float x, float z) {
        const float padTop  = m_terrain->heightAt(x, z);
        const mat4 padModel = glm::translate(
            mat4(1.0f), vec3{x - m_renderOrigin.x, padTop + 0.08f, z - m_renderOrigin.z});
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
    for (const render::Landmark& pad : m_terrain->helipads()) {
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(pad.lon, pad.lat, x, z);
        if (std::fabs(x) <= halfW && std::fabs(z) <= halfH) {
            drawPad(x, z);
        }
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

void Application::drawGroundShadow(const mat4& base, float rotorFraction, const mat4& view,
                                  const mat4& proj, const vec3& sunDir) {
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
    const float shadowAlpha = 0.26f * clamp(1.0f - altitude / 40.0f, 0.0f, 1.0f);
    if (shadowAlpha <= 0.01f) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    /* Posée sur un hélipad, l'ombre (juste au-dessus du sol) se retrouve très
       proche en profondeur du disque du pad : avec le test de profondeur, elle se
       dessinait en damier sur le pad (z-fighting). L'ombre est un décalque au sol
       dessiné AVANT l'appareil ; on désactive donc son test de profondeur, si bien
       qu'elle se fond simplement sur ce qui est déjà au sol (terrain ou pad) sans
       jamais se disputer la profondeur. L'appareil, dessiné après, la recouvre. */
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    const float scaleXZ = 1.0f + altitude * 0.02f;
    constexpr float DISC_MESH_R = 6.0f;  /* rayon du maillage de disque (cf. disc(6, ...)) */

    /*
     * Le disque est plat : sur un sol en pente, il couperait le relief et la
     * ligne d'intersection scintillerait au moindre mouvement de la caméra.
     * On pose donc chaque disque au-dessus du plus haut des quatre coins de son
     * emprise, avec une marge, pour qu'il ne traverse jamais le sol.
     */
    const auto topUnder = [&](float cx, float cz, float r) {
        float t = m_terrain->heightAt(cx, cz);
        t       = std::fmax(t, m_terrain->heightAt(cx + r, cz + r));
        t       = std::fmax(t, m_terrain->heightAt(cx + r, cz - r));
        t       = std::fmax(t, m_terrain->heightAt(cx - r, cz + r));
        t       = std::fmax(t, m_terrain->heightAt(cx - r, cz - r));
        return t;
    };

    /*
     * Ombre projetée par le soleil. sunDir pointe VERS le soleil ; sa composante y
     * est le sinus de sa hauteur. L'ombre tombe à l'opposé du soleil et s'étire
     * quand il est bas :
     *  - dayBlend : 0 au ras de l'horizon et la nuit (l'ombre redevient alors un
     *    simple contact centré sous l'appareil), 1 quand le soleil est bien levé ;
     *  - elong : étirement le long de la direction du soleil (rond à midi, allongé
     *    au ras du sol) ;
     *  - azimuth : oriente le grand axe de l'ellipse sur la direction du soleil ;
     *  - sunShift : décalage horizontal du centre de l'ombre, à l'opposé du soleil,
     *    proportionnel à la hauteur de la source et borné pour rester lisible.
     */
    const float sunY        = sunDir.y;
    const float dayBlend    = clamp((sunY - 0.02f) / 0.20f, 0.0f, 1.0f);
    const float sunYFloor   = std::fmax(sunY, 0.2f);
    const float sunHorizLen = std::sqrt(sunDir.x * sunDir.x + sunDir.z * sunDir.z);
    const float azimuth     = (sunHorizLen > 1e-4f) ? std::atan2(-sunDir.z, sunDir.x) : 0.0f;
    constexpr float ELONG_MAX = 3.5f;
    const float elong       = 1.0f + dayBlend * (clamp(1.0f / sunYFloor, 1.0f, ELONG_MAX) - 1.0f);

    constexpr float SHADOW_SRC_H = 2.5f;   /* hauteur type de la source (rotor/cabine) au-dessus du sol */
    constexpr float MAX_OFFSET   = 28.0f;  /* décalage horizontal max de l'ombre (m) */
    vec3 sunShift{0.0f};
    if (dayBlend > 0.0f && sunHorizLen > 1e-4f) {
        const float dist = std::fmin((altitude + SHADOW_SRC_H) / sunYFloor, MAX_OFFSET) * dayBlend;
        sunShift = dist * vec3{-sunDir.x, 0.0f, -sunDir.z} / sunHorizLen;
    }

    m_shadowShader->use();
    m_shadowShader->setMat4("u_view", view);
    m_shadowShader->setMat4("u_proj", proj);

    /* Centre commun des deux disques : sous l'axe du mât (et non sous le centre de
       l'appareil), pour que l'ombre des pales tombe au bon endroit. Le mât est en
       avant de l'origine, le long de l'axe X du fuselage. */
    vec3 center = heliPos;
    if (m_loadedHeli) {
        center =
            vec3(base * vec4(render::LoadedHelicopter::ROTOR_FORWARD_OFFSET, 0.0f, 0.0f, 1.0f));
    }

    /* Dessine un disque d'ombre (échelle baseScale sur le maillage de rayon 6 m),
       décalé et étiré par le soleil, posé au-dessus du relief. */
    const auto drawDisc = [&](float baseScale, float alpha) {
        const float gx    = center.x + sunShift.x;
        const float gz    = center.z + sunShift.z;
        const float longR = baseScale * elong * DISC_MESH_R;  /* demi-grand axe pour l'échantillon sol */
        const float y     = topUnder(gx, gz, longR) + 0.30f;
        /* Rendu relatif à la caméra : la vue est relative à m_renderOrigin. Le grand
           axe (X local, aligné sur l'azimut solaire) est étiré par elong. */
        const mat4 model = glm::translate(mat4(1.0f), vec3{gx, y, gz} - m_renderOrigin) *
                           glm::rotate(mat4(1.0f), azimuth, vec3{0.0f, 1.0f, 0.0f}) *
                           glm::scale(mat4(1.0f), vec3{baseScale * elong, 1.0f, baseScale});
        m_shadowShader->setMat4("u_model", model);
        m_shadowShader->setFloat("u_alpha", alpha);
        m_shadowDisc->draw();
    };

    /* Disque rotor (l'ombre des pales) : grand, son opacité suit le régime. */
    const float rotorShadowAlpha = shadowAlpha * 0.7f * clamp(rotorFraction, 0.0f, 1.0f);
    if (rotorShadowAlpha > 0.01f) {
        drawDisc(scaleXZ, rotorShadowAlpha);
    }
    /* Fuselage : disque dense toujours présent, dimensionné à l'empreinte posée
       (~5 m), concentrique avec celui du rotor. */
    drawDisc(scaleXZ * (5.0f / 6.0f), shadowAlpha);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

}  /* namespace artouste::app */
