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

void Application::renderTerrainAndBuildings(const RenderContext& ctx) {
    /*
     * Terrain : orthophoto réelle drapée sur le relief, avec brume au loin.
     * Si les données réelles manquent, on retombe sur le shader à couleurs de
     * sommets et le damier plat de repli.
     */
    if (m_terrain->textured()) {
        m_terrainShader->use();
        m_terrainShader->setMat4("u_view", ctx.view);
        m_terrainShader->setMat4("u_proj", ctx.proj);
        m_terrainShader->setMat4("u_model", ctx.toRel);
        m_terrainShader->setVec3("u_lightDir", ctx.lightDir);
        m_terrainShader->setVec3("u_seaColor", SEA_COLOR);
        m_terrainShader->setVec3("u_camPos", ctx.camPosRel);
        m_terrainShader->setVec3("u_fogColor", ctx.fogColor);
        m_terrainShader->setFloat("u_fogStart", m_fogStart);
        m_terrainShader->setFloat("u_fogEnd", m_fogEnd);
        m_terrainShader->setInt("u_texture", 0);
        m_terrainShader->setInt("u_detail", 1);
        m_terrainShader->setVec2("u_originXZ", vec2{m_renderOrigin.x, m_renderOrigin.z});
        m_terrainShader->setFloat("u_orthoMPP", m_terrain->orthoMetersPerPixel());
        if (m_terrainDetail) {
            m_terrainDetail->bind(1);
        }

        /* Tuiles fines autour de la caméra, si la carte en livre : niveau large
           sur les unités 2 et 3, niveau serré sur les unités 4 et 5. Une taille
           de fenêtre nulle éteint le niveau côté shader, ce qui est le cas
           normal d'une carte sans tuiles : il n'y a alors ni texture à attacher
           ni autre uniforme à renseigner. */
        const auto reglerNiveau = [this](const render::tuiles::Fenetre* fenetre,
                                         const char*                   prefixe,
                                         unsigned int                  uniteTuiles,
                                         unsigned int                  uniteMasque) {
            const std::string base = std::string("u_") + prefixe;
            m_terrainShader->setInt(base, static_cast<int>(uniteTuiles));
            m_terrainShader->setInt(base + "Masque", static_cast<int>(uniteMasque));
            if (fenetre == nullptr) {
                m_terrainShader->setFloat(base + "TailleM", 0.0f);
                return;
            }
            fenetre->bind(uniteTuiles, uniteMasque);
            m_terrainShader->setVec2(base + "Ancre", vec2{fenetre->ancreX(), fenetre->ancreZ()});
            m_terrainShader->setFloat(base + "TailleM", fenetre->tailleM());
            m_terrainShader->setFloat(base + "MPP", fenetre->mParPixel());
            m_terrainShader->setFloat(base + "Plein", fenetre->rayonPleinM());
            m_terrainShader->setFloat(base + "Fondu", fenetre->rayonFonduM());
        };
        reglerNiveau(m_terrain->detail(), "fine", 2, 3);
        reglerNiveau(m_terrain->detailFin(), "serre", 4, 5);

        m_terrain->bindTexture(0);

        /* Relief fin autour de l'appareil, dessiné AVANT le maillage d'ensemble
           et marquant son emprise dans le pochoir : le maillage d'ensemble en
           est ensuite écarté. Un simple décalage de profondeur ne suffirait pas,
           le MNT LiDAR et le RGE ALTI s'écartant de plusieurs mètres, et le
           maillage d'ensemble masquerait le relief fin partout où il passe
           au-dessus. */
        if (const render::relief::FenetreRelief* relief = m_terrain->reliefFin();
            relief != nullptr) {
            m_terrainShader->setInt("u_reliefActif", 1);
            m_terrainShader->setInt("u_relief", 6);
            m_terrainShader->setInt("u_carteRelief", 7);
            relief->bind(6);
            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_2D, m_terrain->carteReliefTexId());
            glActiveTexture(GL_TEXTURE0);

            m_terrainShader->setVec2("u_reliefAncre", vec2{relief->ancreX(), relief->ancreZ()});
            m_terrainShader->setVec2("u_reliefTailleM",
                                     vec2{relief->tailleM(), relief->tailleZ()});
            m_terrainShader->setFloat("u_reliefTexels",
                                      static_cast<float>(relief->cotePoints()));
            m_terrainShader->setVec2("u_reliefCentre", vec2{relief->centreX(), relief->centreZ()});
            m_terrainShader->setVec2("u_reliefOeil", vec2{relief->oeilX(), relief->oeilZ()});
            m_terrainShader->setVec2("u_reliefFondu",
                                     vec2{relief->fonduDebutM(), relief->fonduFinM()});
            m_terrainShader->setFloat("u_reliefLissage", relief->niveauLissage());
            m_terrainShader->setVec2("u_reliefPasTexture",
                                     vec2{relief->pasX(), relief->pasZ()});
            m_terrainShader->setFloat("u_reliefDetailM", relief->distanceDetailM());
            m_terrainShader->setVec2("u_carteCoin",
                                     vec2{m_terrain->originX() - m_terrain->halfWidth(),
                                          m_terrain->originZ() - m_terrain->halfHeight()});
            m_terrainShader->setVec2("u_carteTailleM", vec2{2.0f * m_terrain->halfWidth(),
                                                            2.0f * m_terrain->halfHeight()});
            /* ARTOUSTE_DEBUG_RELIEF_RACCORD=0 débraye le raccord au maillage. */
            static const int raccord =
                (std::getenv("ARTOUSTE_DEBUG_RELIEF_RACCORD") != nullptr &&
                 std::atoi(std::getenv("ARTOUSTE_DEBUG_RELIEF_RACCORD")) == 0) ? 0 : 1;
            m_terrainShader->setInt("u_reliefRaccord", raccord);
            m_terrainShader->setInt("u_cartePasMaillage", m_terrain->pasMaillage());
            m_terrainShader->setFloat("u_cartePasNormale", m_terrain->pasNormaleM());
            m_terrainShader->setVec2("u_carteTexels",
                                     vec2{static_cast<float>(m_terrain->gridCols()),
                                          static_cast<float>(m_terrain->gridRows())});

            /* Du noyau vers l'anneau : chacun ne remplit que ce que le
               précédent a laissé, et marque le pochoir à son tour. */
            glEnable(GL_STENCIL_TEST);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            for (int niveau = 0; niveau < relief->niveaux(); ++niveau) {
                glStencilFunc(niveau == 0 ? GL_ALWAYS : GL_NOTEQUAL, 1, 0xFF);
                m_terrainShader->setInt("u_reliefCote", relief->coteGrille(niveau));
                m_terrainShader->setVec2("u_reliefPas",
                                         vec2{relief->pasGrille(niveau),
                                              relief->pasGrilleZ(niveau)});
                relief->dessiner(niveau);
            }

            /* Le maillage d'ensemble ne dessine que là où la fenêtre n'a rien
               posé. */
            m_terrainShader->setInt("u_reliefActif", 0);
            glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            m_terrain->draw();
            glDisable(GL_STENCIL_TEST);
        } else {
            m_terrainShader->setInt("u_reliefActif", 0);
            m_terrain->draw();
        }
    } else {
        m_shader->use();
        m_shader->setMat4("u_view", ctx.view);
        m_shader->setMat4("u_proj", ctx.proj);
        m_shader->setVec3("u_lightDir", ctx.lightDir);
        m_shader->setMat4("u_model", ctx.toRel);
        m_terrain->draw();
    }

    /*
     * Bâtiments 3D extrudés (BD TOPO) : éclairés et noyés dans la même brume que le
     * terrain pour un raccord cohérent au loin. Maillage statique unique ; rien si
     * le terrain n'en fournit pas.
     */
    if (m_buildings && m_buildings->built()) {
        m_buildingShader->use();
        m_buildingShader->setMat4("u_view", ctx.view);
        m_buildingShader->setMat4("u_proj", ctx.proj);
        m_buildingShader->setMat4("u_model", ctx.toRel);
        m_buildingShader->setVec3("u_lightDir", ctx.lightDir);
        m_buildingShader->setVec3("u_camPos", ctx.camPosRel);
        m_buildingShader->setVec3("u_fogColor", ctx.fogColor);
        m_buildingShader->setFloat("u_fogStart", m_fogStart);
        m_buildingShader->setFloat("u_fogEnd", m_fogEnd);
        m_buildingShader->setInt("u_facade", 0);
        m_buildingShader->setInt("u_facadePleine", 1);
        if (m_buildingFacade) {
            m_buildingFacade->bind(0);
        }
        if (m_buildingFacadePleine) {
            m_buildingFacadePleine->bind(1);
        }
        /* Culling par tuiles : le recalage d'origine (u_model = toRel) s'annule dans le
           produit final, donc le frustum en coordonnées monde s'extrait de proj * vue
           monde (m_camera.view()), et la caméra est prise en position monde. */
        m_buildings->draw(ctx.proj * m_camera.view(), m_camera.position());
    }
}

void Application::renderVegetationAndClouds(const RenderContext& ctx) {
    /*
     * Végétation en billboards (arbres) : test alpha et écriture de profondeur
     * (pas de mélange), donc l'ordre de dessin importe peu. Même brume que le
     * terrain et les bâtiments pour un raccord cohérent au loin.
     */
    if (m_vegetation && m_vegetation->built()) {
        m_vegetationShader->use();
        m_vegetationShader->setMat4("u_view", ctx.view);
        m_vegetationShader->setMat4("u_proj", ctx.proj);
        m_vegetationShader->setMat4("u_model", ctx.toRel);
        m_vegetationShader->setVec3("u_lightDir", ctx.lightDir);
        m_vegetationShader->setVec3("u_camPos", ctx.camPosRel);
        m_vegetationShader->setVec3("u_fogColor", ctx.fogColor);
        m_vegetationShader->setFloat("u_fogStart", m_fogStart);
        m_vegetationShader->setFloat("u_fogEnd", m_fogEnd);
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
        m_cloudShader->setMat4("u_view", ctx.view);
        m_cloudShader->setMat4("u_proj", ctx.proj);
        m_cloudShader->setMat4("u_model", ctx.toRel);
        m_cloudShader->setVec3("u_lightDir", ctx.lightDir);
        m_cloudShader->setVec3("u_camPos", ctx.camPosRel);
        m_cloudShader->setVec3("u_fogColor", ctx.fogColor);
        m_cloudShader->setFloat("u_fogStart", m_fogStart);
        m_cloudShader->setFloat("u_fogEnd", m_fogEnd);
        m_cloudShader->setInt("u_texture", 0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        m_clouds->draw(m_camera.position());
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

} /* namespace artouste::app */
