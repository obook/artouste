/*
 * ApplicationGroundShadow.cpp
 * Ombre portée de l'appareil au sol : deux disques (fuselage et rotor),
 * décalés et étirés selon la position du soleil ou de la lune. Complète
 * ApplicationGround.cpp (hélipads, traces de brûlure) et
 * ApplicationGroundHapi.cpp (balises HAPI) : trois décalques posés au sol
 * avant l'appareil, chacun avec sa propre logique de placement.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "render/Camera.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Mesh.hpp"
#include "render/Shader.hpp"
#include "render/Terrain.hpp"
#include "util/Math.hpp"

#include <glad/glad.h>

#include <cmath>

namespace artouste::app {

void Application::drawGroundShadow(
    const mat4& base, float rotorFraction, const mat4& view, const mat4& proj, const vec3& sunDir) {
    /*
     * Ombre portée, en deux disques de taille fixe (pas d'animation de taille) :
     *  - le fuselage : ellipse dense, toujours présente ;
     *  - le rotor : grand disque plus clair dont seule l'opacité suit le régime,
     *    invisible rotor arrêté. Ainsi l'ombre reste cohérente avec l'état des
     *    pales sans grandir ni rétrécir. Les deux s'estompent avec l'altitude.
     */
    const vec3 heliPos = vec3(base[3]);
    const float ground = m_terrain->heightAt(heliPos.x, heliPos.z);
    const float altitude = heliPos.y - ground > 0.0f ? heliPos.y - ground : 0.0f;
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
    constexpr float DISC_MESH_R = 6.0f; /* rayon du maillage de disque (cf. disc(6, ...)) */

    /*
     * Le disque est plat : sur un sol en pente, il couperait le relief et la
     * ligne d'intersection scintillerait au moindre mouvement de la caméra.
     * On pose donc chaque disque au-dessus du plus haut des quatre coins de son
     * emprise, avec une marge, pour qu'il ne traverse jamais le sol.
     */
    const auto topUnder = [&](float cx, float cz, float r) {
        float t = m_terrain->heightAt(cx, cz);
        t = std::fmax(t, m_terrain->heightAt(cx + r, cz + r));
        t = std::fmax(t, m_terrain->heightAt(cx + r, cz - r));
        t = std::fmax(t, m_terrain->heightAt(cx - r, cz + r));
        t = std::fmax(t, m_terrain->heightAt(cx - r, cz - r));
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
     *  - elong : étirement le long de la direction de l'astre (rond au zénith, long au ras du sol)
     * ;
     *  - azimuth : oriente le grand axe de l'ellipse ;
     *  - lightShift : décalage du centre de l'ombre, à l'opposé de l'astre, borné.
     */
    const bool sunUp = sunDir.y > 0.0f;
    const vec3 lightVec = sunUp ? sunDir : -sunDir; /* lune = direction opposée au soleil */
    const float lightY = lightVec.y;                /* hauteur de l'astre éclairant */
    const float lightBlend = clamp((lightY - 0.02f) / 0.20f, 0.0f, 1.0f);
    const float lightYFloor = std::fmax(lightY, 0.2f);
    const float lightHorizLen = std::sqrt(lightVec.x * lightVec.x + lightVec.z * lightVec.z);
    const float azimuth = (lightHorizLen > 1e-4f) ? std::atan2(-lightVec.z, lightVec.x) : 0.0f;
    constexpr float ELONG_MAX = 3.5f;
    const float elong = 1.0f + lightBlend * (clamp(1.0f / lightYFloor, 1.0f, ELONG_MAX) - 1.0f);

    /* Intensité : pleine au soleil, atténuée à la lune ; plancher de contact pour que
       l'appareil reste posé même au crépuscule (astre au ras de l'horizon). */
    constexpr float MOON_SHADOW = 0.30f;   /* part de l'ombre lunaire par rapport au plein soleil */
    constexpr float CONTACT_FLOOR = 0.15f; /* ombre de contact minimale (anti-flottement) */
    const float alphaMult = std::fmax(CONTACT_FLOOR, lightBlend * (sunUp ? 1.0f : MOON_SHADOW));

    constexpr float SHADOW_SRC_H =
        2.5f; /* hauteur type de la source (rotor/cabine) au-dessus du sol */
    constexpr float MAX_OFFSET = 28.0f; /* décalage horizontal max de l'ombre (m) */
    vec3 lightShift{0.0f};
    if (lightBlend > 0.0f && lightHorizLen > 1e-4f) {
        const float dist =
            std::fmin((altitude + SHADOW_SRC_H) / lightYFloor, MAX_OFFSET) * lightBlend;
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
        const float gx = center.x + lightShift.x;
        const float gz = center.z + lightShift.z;
        const float longR =
            baseScale * xElong * DISC_MESH_R; /* demi-grand axe pour l'échantillon sol */
        const float y = topUnder(gx, gz, longR) + 0.30f;
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
    const float rotorShadowAlpha =
        shadowAlpha * 0.7f * clamp(rotorFraction, 0.0f, 1.0f) * alphaMult;
    if (ROTOR_SHADOW_ENABLED && rotorShadowAlpha > 0.01f) {
        drawEllipse(scaleXZ, rotorShadowAlpha, azimuth, elong);
    }

    /* Fuselage : ellipse dense toujours présente, orientée selon le cap de
       l'appareil (et non le soleil) pour épouser grossièrement la silhouette
       allongée de la cabine, plutôt qu'un disque rond. */
    constexpr float CABIN_ELONG = 1.6f; /* rapport longueur/largeur de l'ellipse cabine */
    const vec3 fwd = glm::normalize(vec3(base[0]));
    const float heading = std::atan2(-fwd.z, fwd.x);
    drawEllipse(scaleXZ * (2.0f / 6.0f), shadowAlpha * alphaMult, heading, CABIN_ELONG);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} /* namespace artouste::app */
