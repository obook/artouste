/*
 * ApplicationRenderWorld.cpp
 * Rendu du décor statique d'une image : ciel et plan de mer, terrain et
 * bâtiments, végétation et nuages. Les entités dynamiques (mode zombie,
 * hélicoptère) sont dans ApplicationRenderActors.cpp ; l'orchestration dans
 * ApplicationRender.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/AppConstants.hpp"
#include "app/Application.hpp"
#include "render/Buildings.hpp"
#include "render/Camera.hpp"
#include "render/Clouds.hpp"
#include "render/Mesh.hpp"
#include "render/Shader.hpp"
#include "render/Skybox.hpp"
#include "render/Terrain.hpp"
#include "render/Texture.hpp"
#include "render/Vegetation.hpp"

#include <glad/glad.h>

#include <cstdlib>

namespace artouste::app {

void Application::renderSkyAndSea(const RenderContext& ctx, float timeSeconds) {
    /* Ciel en dégradé (il remplit le fond de l'image). On passe l'inverse de
       (projection * rotation caméra seule) : en retirant la translation (position
       caméra, en milliers de mètres), le ciel reconstruit la direction du rayon sans
       soustraction de grands nombres, ce qui supprime le tremblement du soleil. */
    /* La lune est modélisée à l'opposé du soleil (voir drawGroundShadow) : elle est
       donc levée quand le soleil est couché. */
    m_sky->draw(*m_skyShader,
                glm::inverse(ctx.proj * mat4(mat3(ctx.view))),
                ctx.lightDir,
                -ctx.lightDir,
                timeSeconds);

    /* Plan de mer : grand quadrilatère bleu qui se perd dans la brume au loin.
     * Il est toujours sous la mer du terrain (dessinée à y=0) et n'a jamais à
     * occulter le terrain ; on le dessine donc sans écrire dans le tampon de
     * profondeur, de sorte que le terrain (dessiné après) le recouvre toujours.
     * Cela supprime le z-fighting au loin, y compris en vue cockpit où le faible
     * plan rapproché (near) dégrade fortement la précision de profondeur. */
    /* En montagne (terrain sans mer), on ne dessine pas le plan de mer. */
    if (m_terrain->drawsSea()) {
        m_seaShader->use();
        m_seaShader->setMat4("u_view", ctx.view);
        m_seaShader->setMat4("u_proj", ctx.proj);
        m_seaShader->setMat4("u_model", ctx.toRel);
        m_seaShader->setVec3("u_seaColor", SEA_COLOR);
        m_seaShader->setVec3("u_lightDir", ctx.lightDir);
        m_seaShader->setVec3("u_camPos", ctx.camPosRel);
        m_seaShader->setVec3("u_fogColor", ctx.fogColor);
        m_seaShader->setFloat("u_fogStart", m_fogStart);
        m_seaShader->setFloat("u_fogEnd", m_fogEnd);
        glDepthMask(GL_FALSE);
        m_sea->draw();
        glDepthMask(GL_TRUE);
    }
}

} /* namespace artouste::app */
