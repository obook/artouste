/*
 * ApplicationRenderTerrain.cpp
 * Terrain texturé, fenêtres de détail et de relief, bâtiments extrudés.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "app/AppConstants.hpp"

#include "render/Buildings.hpp"
#include "render/Clouds.hpp"
#include "render/Shader.hpp"
#include "render/Terrain.hpp"
#include "render/Vegetation.hpp"

#include <glad/glad.h>

#include <cmath>
#include <cstdlib>

namespace artouste::app {

namespace {

/* Largeur au sol des liserets de diagnostic, en mètres : celle de l'essai qui a
   servi à régler la fenêtre. Assez large pour se voir à deux kilomètres, assez
   fine pour ne rien cacher du sol. */
constexpr float LISERET_M = 10.0f;

/* Demi-côtés au sol d'une grille de la fenêtre, par axe. La grille n'est pas
   carrée : ses deux pas diffèrent de quelques pour cent. */
[[nodiscard]] vec2 demiGrille(const render::relief::FenetreRelief& relief, int niveau) {
    const float points = 0.5f * static_cast<float>(relief.coteGrille(niveau) - 1);
    return vec2{points * relief.pasGrille(niveau), points * relief.pasGrilleZ(niveau)};
}

} /* namespace */

void Application::renderTerrainAndBuildings(const RenderContext& ctx) {
    /* ARTOUSTE_DEBUG_SONDE : le terrain écrit une mesure au lieu de sa
       couleur. En capture seulement. À retirer. */
    static const int sonde = [] {
        const char* e = std::getenv("ARTOUSTE_DEBUG_SONDE");
        return (e != nullptr) ? std::atoi(e) : 0;
    }();

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
        m_terrainShader->setInt("u_sonde", sonde);
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

        /* Chaussée des ouvrages d'art, dessinée avec le TERRAIN et non avec les
           bâtiments : elle reçoit ainsi l'orthophoto et les tuiles de détail du
           sol, drapées par coordonnées monde. Le tablier montre donc la photo du
           pont, à la finesse du sol d'à-côté, au lieu d'un aplat gris qui
           laissait voir la photo du pont dépasser autour de lui.
           Tirée APRÈS le maillage d'ensemble et hors du pochoir : dans le
           pochoir elle disparaîtrait sous l'emprise de la fenêtre de relief,
           c'est-à-dire juste sous l'appareil. */
        const auto dessinerTabliers = [this] {
            if (m_buildings && m_buildings->aTabliers()) {
                m_buildings->drawTabliers();
            }
        };

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
            /* Liserets de diagnostic (clé "relief_debug") : les contours des
               deux grilles, posés une fois pour toute la carte. Ils restent en
               place pour le maillage d'ensemble, dont le trait de l'anneau
               marque justement la frontière. Sans anneau, le second contour se
               confond avec le premier et ne se voit pas. */
            m_terrainShader->setFloat("u_reliefLiseret",
                                      m_config.reliefDebug ? LISERET_M : 0.0f);
            const int large = (relief->niveaux() > 1) ? 1 : 0;
            m_terrainShader->setVec2("u_reliefDemiNoyau", demiGrille(*relief, 0));
            m_terrainShader->setVec2("u_reliefDemiAnneau", demiGrille(*relief, large));
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

            /* De l'ANNEAU vers le noyau, chacun sur toute son emprise, le plus
               fin par-dessus. Le pochoir ne sert donc plus qu'à écarter le
               maillage d'ensemble ; entre les deux grilles, c'est la profondeur
               qui tranche.

               L'ordre compte. Tiré d'abord, le noyau laissait des trous : sur une
               arête vue en rasant, ses triangles de 2 m se projettent en lamelles
               qui ne couvrent aucun centre de pixel, rien n'était écrit, le
               pochoir restait libre et l'anneau y montrait sa corde, plus basse.
               Il en naissait un peigne de lames sous la ligne de crête, une par
               cellule d'anneau (mesuré sur ossau et cauterets le 18/08/2026 :
               11 et 7 traversées de silhouette sur une ligne, contre 1 et 0
               après). Tiré en second, le noyau recouvre l'anneau là où il est
               visible, et ses manques laissent voir une surface continue au lieu
               d'un trou.

               GL_LEQUAL pour le noyau, et non un décalage de profondeur : les
               deux grilles lisent le même champ à leur jonction, leurs
               profondeurs y sont donc quasi égales. Un décalage n'y suffit pas,
               c'est la raison d'être du pochoir (décision du 16/08/2026) ; en
               revanche, à égalité, le dernier tiré gagne, ce qui donne un
               vainqueur déterministe sans rien décaler. */
            glEnable(GL_STENCIL_TEST);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            for (int niveau = relief->niveaux() - 1; niveau >= 0; --niveau) {
                glDepthFunc(niveau == 0 ? GL_LEQUAL : GL_LESS);
                m_terrainShader->setInt("u_reliefCote", relief->coteGrille(niveau));
                m_terrainShader->setVec2("u_reliefPas",
                                         vec2{relief->pasGrille(niveau),
                                              relief->pasGrilleZ(niveau)});
                relief->dessiner(niveau);
            }
            glDepthFunc(GL_LESS);

            /* Le maillage d'ensemble ne dessine que là où la fenêtre n'a rien
               posé. */
            m_terrainShader->setInt("u_reliefActif", 0);
            glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            m_terrain->draw();
            glDisable(GL_STENCIL_TEST);
            dessinerTabliers();
        } else {
            /* Sans fenêtre de relief, il n'y a aucune frontière à tracer. */
            m_terrainShader->setInt("u_reliefActif", 0);
            m_terrainShader->setFloat("u_reliefLiseret", 0.0f);
            m_terrain->draw();
            dessinerTabliers();
        }
    } else {
        m_shader->use();
        m_shader->setFloat("u_alpha", 1.0f);
        m_shader->setVec3("u_tint", vec3{1.0f});
        m_shader->setFloat("u_texMix", 0.0f);
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
