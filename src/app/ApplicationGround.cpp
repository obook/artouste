/*
 * ApplicationGround.cpp
 * Décalques posés au sol et dessinés avant l'appareil : les hélipads (départ et
 * ceux du terrain), les balises HAPI et l'ombre portée de l'appareil. Les
 * hélipads et l'ombre gardent le test de profondeur (le relief peut donc les
 * cacher) mais tirent leur profondeur vers la caméra (polygon offset) pour
 * éviter le z-fighting au ras du relief ; ils n'écrivent pas la profondeur et
 * l'appareil, dessiné après, les recouvre.
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
        const float padTop  = m_terrain->heightAt(x, z);
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
            m_shader->setMat4("u_model", glm::translate(
                mat4(1.0f), vec3{x - m_renderOrigin.x, padTop + 0.06f, z - m_renderOrigin.z}));
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
    for (const render::Landmark& pad : m_terrain->helipads()) {
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(pad.lon, pad.lat, x, z);
        if (std::fabs(x) <= halfW && std::fabs(z) <= halfH) {
            drawPad(x, z);
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);
    glDepthMask(GL_TRUE);
}

void Application::drawHapi(const mat4& view, const mat4& proj, float timeSeconds) {
    /*
     * Balise HAPI : contrairement à l'optique réelle (qui projette 4 faisceaux
     * colorés visibles chacun depuis une plage d'angle différente), on calcule
     * ici l'angle d'élévation entre la balise et l'appareil du joueur (seul
     * point de vue à simuler) et on n'affiche qu'UNE lueur, dans la couleur du
     * secteur correspondant. C'est fidèle au réel : depuis une position donnée,
     * le pilote ne voit jamais qu'une seule des 4 couleurs. Seuils repris du
     * guide DGAC "Installation, exploitation et maintenance du HAPI" (§ 4.4,
     * media/gt_installation_hapi.pdf) : secteur "sur la pente" à 45' d'ouverture
     * de part et d'autre du centre, secteur "légèrement trop bas" de 15' de plus.
     * L'ouverture azimutale du faisceau (~10 % de divergence dans le guide)
     * n'est pas simulée : la balise reste visible dans son secteur vertical
     * quel que soit l'azimut d'approche (piste à consolider).
     */
    /* Dôme 3D masqué pour l'instant : le point d'étiquette (HUD, voir
       ApplicationHud.cpp) suffit à lire la pente sans la lueur au sol. Remettre à
       true pour la réactiver. */
    constexpr bool DRAW_GLOW = false;
    if (!DRAW_GLOW) {
        return;
    }

    const std::vector<render::HapiUnit>& units = m_terrain->hapiUnits();
    if (units.empty()) {
        return;
    }

    /* Rayon exagéré (le boîtier réel tient dans la main) pour rester visible en
       approche à quelques centaines de mètres, faute de billboard à taille
       d'écran constante. */
    constexpr float HAPI_CORE_RADIUS_M = 4.0f;
    constexpr float HAPI_HALO_RADIUS_M = 9.0f;
    const vec4  GREEN{0.05f, 1.0f, 0.15f, 0.95f};
    const vec4  RED{1.0f, 0.08f, 0.08f, 0.95f};
    const vec4  OFF{0.0f, 0.0f, 0.0f, 0.0f};

    const vec3 acPos  = m_flight.body().position;
    const float halfW = m_terrain->halfWidth();
    const float halfH = m_terrain->halfHeight();

    m_flatShader->use();
    m_flatShader->setMat4("u_view", view);
    m_flatShader->setMat4("u_proj", proj);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    const auto drawGlow = [&](const vec3& worldPos, float radius, const vec4& color) {
        if (color.a <= 0.01f) {
            return;
        }
        m_flatShader->setMat4("u_model", glm::translate(mat4(1.0f), worldPos - m_renderOrigin) *
                                             glm::scale(mat4(1.0f), vec3{radius}));
        m_flatShader->setVec4("u_color", color);
        m_glowSphere->draw();
    };

    for (const render::HapiUnit& hapi : units) {
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(hapi.lon, hapi.lat, x, z);
        if (std::fabs(x) > halfW || std::fabs(z) > halfH) {
            continue;  /* balise hors de l'emprise courante */
        }
        const vec3 hapiPos{x, m_terrain->heightAt(x, z) + render::HAPI_MAST_M, z};

        const float dx        = acPos.x - hapiPos.x;
        const float dz        = acPos.z - hapiPos.z;
        const float horizDist = std::sqrt(dx * dx + dz * dz);
        const render::HapiSector sector = render::hapiSector(hapi, horizDist, acPos.y - hapiPos.y);
        const render::HapiGlow   glow   = render::hapiGlow(sector, timeSeconds);
        const vec4 color = glow.off ? OFF : (glow.green ? GREEN : RED);

        drawGlow(hapiPos, HAPI_CORE_RADIUS_M, color);
        drawGlow(hapiPos, HAPI_HALO_RADIUS_M, vec4{vec3{color}, color.a * 0.25f});
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Application::drawGroundShadow(const mat4& base, float rotorFraction, const mat4& view,
                                  const mat4& proj, const vec3& sunDir) {
    /*
     * Ombre portée, en deux disques de taille fixe (pas d'animation de taille) :
     *  - le fuselage : ellipse dense, toujours présente ;
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
       proche en profondeur du sol : au test de profondeur brut, elle se dessinait
       en damier (z-fighting). Même parade que pour le pad : test conservé mais
       profondeur tirée vers la caméra (polygon offset), pour que l'ombre gagne
       contre le sol qui la porte sans recouvrir le relief qui la cache. (Le pad,
       lui, n'écrit pas la profondeur : l'ombre ne se dispute donc qu'avec le
       terrain.) L'appareil, dessiné après, la recouvre. */
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
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
     * Ombre projetée par l'astre au-dessus de l'horizon : le soleil le jour, et une
     * lune simplifiée la nuit (modélisée à l'opposé du soleil, donc levée quand le
     * soleil est couché). L'ombre tombe à l'opposé de cet astre et s'étire quand il
     * est bas. L'ombre lunaire est bien plus faible que l'ombre solaire ; un plancher
     * de contact subsiste toujours pour ne pas faire flotter l'appareil au crépuscule.
     *  - lightVec : direction (normalisée) vers l'astre éclairant, au-dessus de l'horizon ;
     *  - lightBlend : 0 au ras de l'horizon, 1 quand l'astre est bien levé ;
     *  - elong : étirement le long de la direction de l'astre (rond au zénith, long au ras du sol) ;
     *  - azimuth : oriente le grand axe de l'ellipse ;
     *  - lightShift : décalage du centre de l'ombre, à l'opposé de l'astre, borné.
     */
    const bool  sunUp         = sunDir.y > 0.0f;
    const vec3  lightVec      = sunUp ? sunDir : -sunDir;   /* lune = direction opposée au soleil */
    const float lightY        = lightVec.y;                 /* hauteur de l'astre éclairant */
    const float lightBlend    = clamp((lightY - 0.02f) / 0.20f, 0.0f, 1.0f);
    const float lightYFloor   = std::fmax(lightY, 0.2f);
    const float lightHorizLen = std::sqrt(lightVec.x * lightVec.x + lightVec.z * lightVec.z);
    const float azimuth = (lightHorizLen > 1e-4f) ? std::atan2(-lightVec.z, lightVec.x) : 0.0f;
    constexpr float ELONG_MAX = 3.5f;
    const float elong = 1.0f + lightBlend * (clamp(1.0f / lightYFloor, 1.0f, ELONG_MAX) - 1.0f);

    /* Intensité : pleine au soleil, atténuée à la lune ; plancher de contact pour que
       l'appareil reste posé même au crépuscule (astre au ras de l'horizon). */
    constexpr float MOON_SHADOW   = 0.30f;  /* part de l'ombre lunaire par rapport au plein soleil */
    constexpr float CONTACT_FLOOR = 0.15f;  /* ombre de contact minimale (anti-flottement) */
    const float alphaMult = std::fmax(CONTACT_FLOOR, lightBlend * (sunUp ? 1.0f : MOON_SHADOW));

    constexpr float SHADOW_SRC_H = 2.5f;   /* hauteur type de la source (rotor/cabine) au-dessus du sol */
    constexpr float MAX_OFFSET   = 28.0f;  /* décalage horizontal max de l'ombre (m) */
    vec3 lightShift{0.0f};
    if (lightBlend > 0.0f && lightHorizLen > 1e-4f) {
        const float dist = std::fmin((altitude + SHADOW_SRC_H) / lightYFloor, MAX_OFFSET) * lightBlend;
        lightShift = dist * vec3{-lightVec.x, 0.0f, -lightVec.z} / lightHorizLen;
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

    /* Dessine une ellipse d'ombre (échelle baseScale sur le maillage de rayon 6 m,
       étirée d'un facteur xElong le long de l'axe tourné de rotAngle), posée
       au-dessus du relief. */
    const auto drawEllipse = [&](float baseScale, float alpha, float rotAngle, float xElong) {
        const float gx    = center.x + lightShift.x;
        const float gz    = center.z + lightShift.z;
        const float longR = baseScale * xElong * DISC_MESH_R;  /* demi-grand axe pour l'échantillon sol */
        const float y     = topUnder(gx, gz, longR) + 0.30f;
        /* Rendu relatif à la caméra : la vue est relative à m_renderOrigin. */
        const mat4 model = glm::translate(mat4(1.0f), vec3{gx, y, gz} - m_renderOrigin) *
                           glm::rotate(mat4(1.0f), rotAngle, vec3{0.0f, 1.0f, 0.0f}) *
                           glm::scale(mat4(1.0f), vec3{baseScale * xElong, 1.0f, baseScale});
        m_shadowShader->setMat4("u_model", model);
        m_shadowShader->setFloat("u_alpha", alpha);
        m_shadowDisc->draw();
    };

    /* Disque rotor (l'ombre des pales) : grand, son opacité suit le régime, étiré
       vers le soleil (azimuth/elong). Neutralisé pour l'instant (ROTOR_SHADOW_ENABLED
       à false), cela n'apportait rien à l'écran ; le calcul reste en place pour une
       réactivation rapide. */
    constexpr bool ROTOR_SHADOW_ENABLED = false;
    const float    rotorShadowAlpha =
        shadowAlpha * 0.7f * clamp(rotorFraction, 0.0f, 1.0f) * alphaMult;
    if (ROTOR_SHADOW_ENABLED && rotorShadowAlpha > 0.01f) {
        drawEllipse(scaleXZ, rotorShadowAlpha, azimuth, elong);
    }

    /* Fuselage : ellipse dense toujours présente, orientée selon le cap de
       l'appareil (et non le soleil) pour épouser grossièrement la silhouette
       allongée de la cabine, plutôt qu'un disque rond. */
    constexpr float CABIN_ELONG = 1.6f;  /* rapport longueur/largeur de l'ellipse cabine */
    const vec3      fwd         = glm::normalize(vec3(base[0]));
    const float     heading     = std::atan2(-fwd.z, fwd.x);
    drawEllipse(scaleXZ * (2.0f / 6.0f), shadowAlpha * alphaMult, heading, CABIN_ELONG);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

}  /* namespace artouste::app */
