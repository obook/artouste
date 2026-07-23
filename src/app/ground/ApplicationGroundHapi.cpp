/*
 * ApplicationGroundHapi.cpp
 * Balises HAPI (indicateur de pente d'approche) : une lueur au sol par
 * balise, recolorée selon la position de l'appareil par rapport à la pente
 * visée. Complète ApplicationGround.cpp (hélipads, traces de brûlure) et
 * ApplicationGroundShadow.cpp (ombre portée) : trois décalques posés au sol
 * avant l'appareil, chacun avec sa propre logique de placement.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "render/Camera.hpp"
#include "render/Mesh.hpp"
#include "render/Shader.hpp"
#include "render/Terrain.hpp"
#include "util/Math.hpp"

#include <glad/glad.h>

#include <cmath>

namespace artouste::app {

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
    const vec4 GREEN{0.05f, 1.0f, 0.15f, 0.95f};
    const vec4 RED{1.0f, 0.08f, 0.08f, 0.95f};
    const vec4 OFF{0.0f, 0.0f, 0.0f, 0.0f};

    const vec3 acPos = m_flight.body().position;
    const float halfW = m_terrain->halfWidth();
    const float halfH = m_terrain->halfHeight();
    const float origX = m_terrain->originX();
    const float origZ = m_terrain->originZ();

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
        m_flatShader->setMat4("u_model",
                              glm::translate(mat4(1.0f), worldPos - m_renderOrigin) *
                                  glm::scale(mat4(1.0f), vec3{radius}));
        m_flatShader->setVec4("u_color", color);
        m_glowSphere->draw();
    };

    for (const render::HapiUnit& hapi : units) {
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(hapi.lon, hapi.lat, x, z);
        if (std::fabs(x - origX) > halfW || std::fabs(z - origZ) > halfH) {
            continue; /* balise hors de l'emprise courante */
        }
        const vec3 hapiPos{x, m_terrain->heightAt(x, z) + render::HAPI_MAST_M, z};

        const float dx = acPos.x - hapiPos.x;
        const float dz = acPos.z - hapiPos.z;
        const float horizDist = std::sqrt(dx * dx + dz * dz);
        const render::HapiSector sector = render::hapiSector(hapi, horizDist, acPos.y - hapiPos.y);
        const render::HapiGlow glow = render::hapiGlow(sector, timeSeconds);
        const vec4 color = glow.off ? OFF : (glow.green ? GREEN : RED);

        drawGlow(hapiPos, HAPI_CORE_RADIUS_M, color);
        drawGlow(hapiPos, HAPI_HALO_RADIUS_M, vec4{vec3{color}, color.a * 0.25f});
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} /* namespace artouste::app */
