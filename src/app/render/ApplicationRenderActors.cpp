/*
 * ApplicationRenderActors.cpp
 * Rendu des entités dynamiques d'une image : mode zombie (personnages
 * skinnés, pneus toxiques, explosions 3D des roquettes) et l'hélicoptère
 * (modèle FlightGear texturé ou repli procédural). Le décor statique est dans
 * ApplicationRenderWorld.cpp et ApplicationRenderTerrain.cpp ; l'orchestration
 * dans ApplicationRender.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/AppConstants.hpp"
#include "app/Application.hpp"
#include "app/combat/BonusSphereReglages.hpp"
#include "render/HelicopterModel.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Shader.hpp"
#include "render/Texture.hpp"
#include "render/combat/ExplosionFx.hpp"
#include "render/combat/Projectiles.hpp"
#include "render/combat/SkinnedZombies.hpp"
#include "render/combat/ZombieEyes.hpp"
#include "util/Math.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace artouste::app {

void Application::renderCombatEntities(const RenderContext& ctx, float timeSeconds) {
    /*
     * Bonus du kill (sphère posée, et fusée qui l'apporte) : dessinés en
     * premier, tant que l'état de rendu est encore celui du décor -- les
     * zombies, les yeux et les explosions qui suivent le modifient. Ils gardent
     * l'écriture de profondeur, même en fin de vie où ils s'effacent : sans
     * elle, l'appareil dessiné plus tard passerait devant un bonus pourtant plus
     * proche. Les faces arrière se mélangent alors deux fois par endroits une
     * fois le fondu commencé, ce qui l'assombrit un peu -- sans conséquence pour
     * un repère.
     */
    if (m_combat.active() && m_bonusSphere) {
        const std::vector<CombatMode::BonusSphereView> spheres = m_combat.bonusSpheres();
        if (!spheres.empty()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            /* Bonus posés : dessinés avec le shader éclairé (m_shader), sinon
               une sphère d'une seule couleur se lirait comme un disque plat.
               La teinte et l'opacité du fondu de fin passent par u_tint et
               u_alpha. */
            m_shader->use();
            m_shader->setMat4("u_view", ctx.view);
            m_shader->setMat4("u_proj", ctx.proj);
            m_shader->setVec3("u_lightDir", ctx.lightDir);
            /* Lettrage qui tourne autour de la sphère : le décalage d'UV fait
               défiler le texte sur la surface, sans faire tourner la géométrie
               (les pôles resteraient fixes de toute façon). */
            m_shader->setInt("u_texture", 0);
            m_shader->setFloat("u_uvSpin", -timeSeconds * BONUS_TEXTE_TOURS_S);
            for (const CombatMode::BonusSphereView& sphere : spheres) {
                if (sphere.enVol) {
                    continue;
                }
                const render::Texture* lettrage = m_bonusTexteCarburant.get();
                vec3                   teinte   = BONUS_SPHERE_COLOR_CARBURANT;
                if (sphere.type == CombatMode::BonusType::Vie) {
                    lettrage = m_bonusTexteSante.get();
                    teinte   = BONUS_SPHERE_COLOR_SANTE;
                } else if (sphere.type == CombatMode::BonusType::Mort) {
                    lettrage = m_bonusTexteMort.get();
                    teinte   = BONUS_SPHERE_COLOR_MORT;
                }
                if (lettrage != nullptr) {
                    lettrage->bind(0);
                }
                m_shader->setFloat("u_texMix", lettrage != nullptr ? 1.0f : 0.0f);
                m_shader->setVec3("u_tint", teinte);
                m_shader->setFloat("u_alpha", sphere.alpha);
                m_shader->setMat4("u_model", ctx.toRel *
                                                 glm::translate(mat4(1.0f), sphere.center) *
                                                 glm::scale(mat4(1.0f), vec3{sphere.scale}));
                m_bonusSphere->draw();
            }

            /* Retour au shader plat pour la fusée et sa flamme. */
            m_flatShader->use();
            m_flatShader->setMat4("u_view", ctx.view);
            m_flatShader->setMat4("u_proj", ctx.proj);

            /* Fusées en route : le tube noir opaque, puis la flamme qui sort par
               l'arrière tant que le moteur pousse -- lueur additive, profondeur
               lue mais pas écrite, comme les autres feux du mode (voir
               ApplicationRenderEffects). La flamme bat légèrement pour ne pas
               paraître peinte, et s'éteint dès la retombée. */
            m_flatShader->setVec4("u_color", vec4{0.05f, 0.05f, 0.06f, 1.0f});
            for (const CombatMode::BonusSphereView& sphere : spheres) {
                if (!sphere.enVol) {
                    continue;
                }
                m_flatShader->setMat4("u_model",
                                      ctx.toRel * glm::translate(mat4(1.0f), sphere.center));
                m_bonusRocket->draw();
            }

            const float flamme =
                BONUS_ROCKET_FLAME_M *
                (0.85f + 0.15f * std::sin(timeSeconds * BONUS_ROCKET_FLAME_HZ));
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDepthMask(GL_FALSE);
            m_flatShader->setVec4("u_color", vec4{BONUS_ROCKET_FLAME_COLOR, 0.9f});
            for (const CombatMode::BonusSphereView& sphere : spheres) {
                if (!sphere.propulsion || !m_glowSphere) {
                    continue;
                }
                const vec3 arriere = sphere.center - vec3{0.0f, BONUS_ROCKET_LEN_M, 0.0f};
                m_flatShader->setMat4("u_model", ctx.toRel *
                                                     glm::translate(mat4(1.0f), arriere) *
                                                     glm::scale(mat4(1.0f), vec3{flamme}));
                m_glowSphere->draw();
            }
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
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
        m_zombieShader->setMat4("u_model", ctx.toRel);
        m_zombieShader->setMat4("u_view", ctx.view);
        m_zombieShader->setMat4("u_proj", ctx.proj);
        m_zombieShader->setVec3("u_lightDir", ctx.lightDir);
        m_zombieShader->setVec3("u_camPos", ctx.camPosRel);
        m_zombieShader->setVec3("u_fogColor", ctx.fogColor);
        m_zombieShader->setFloat("u_fogStart", m_fogStart);
        m_zombieShader->setFloat("u_fogEnd", m_fogEnd);
        m_zombieShader->setInt("u_texture", 0);
        m_zombiesRender->updateInstances(
            m_combat.zombieTransforms(), m_combat.zombieHitFlashes(), m_combat.zombieKinds());
        m_zombiesRender->draw(*m_zombieShader, timeSeconds);
    }

    /*
     * Lueur des yeux : deux billboards additifs par zombie, dessinés APRÈS les
     * personnages pour se superposer à leur visage (profondeur lue, pas écrite,
     * voir ZombieEyes::draw). Verte pour un marcheur, rouge pour un largueur,
     * ce qui signale le boss de loin.
     *
     * Les deux points viennent du pack skinné (SkinnedZombies::eyeAnchors), qui
     * les tient sur l'os de tête de la pose qu'il vient de dessiner -- d'où
     * l'ordre : les zombies au-dessus, les yeux ici. Un point fixe du repère du
     * modèle ne suffisait pas : la tête animée s'en écarte de plus de sa propre
     * taille, et les lueurs flottaient à côté du visage. La horde ne fournit que
     * la couleur et le rayon (zombieEyeTints), dans le même ordre que les
     * matrices et les "kind".
     */
    if (m_combat.active() && m_zombieEyesRender && m_zombieEyesRender->built() && m_zombiesRender &&
        m_zombiesRender->built()) {
        const auto transforms = m_combat.zombieTransforms();
        const auto kinds      = m_combat.zombieKinds();
        const auto tints      = m_combat.zombieEyeTints();
        const std::size_t n   = std::min({transforms.size(), kinds.size(), tints.size()});

        std::vector<render::ZombieEyes::Instance> instances;
        instances.reserve(n * 2);
        for (std::size_t i = 0; i < n; ++i) {
            if (tints[i].color == vec3(0.0f)) {
                continue;  /* regard éteint (fin de chute) : pas de lueur du tout */
            }
            vec3 left{0.0f};
            vec3 right{0.0f};
            if (!m_zombiesRender->eyeAnchors(kinds[i], left, right)) {
                continue;  /* variante sans os de tête repéré au chargement */
            }
            for (const vec3& local : {left, right}) {
                render::ZombieEyes::Instance inst;
                inst.posRadius = vec4{vec3(transforms[i] * vec4{local, 1.0f}), tints[i].radius};
                inst.color     = vec4{tints[i].color, 0.0f};
                instances.push_back(inst);
            }
        }

        if (!instances.empty()) {
            m_zombieEyesShader->use();
            m_zombieEyesShader->setMat4("u_model", ctx.toRel);
            m_zombieEyesShader->setMat4("u_view", ctx.view);
            m_zombieEyesShader->setMat4("u_proj", ctx.proj);
            m_zombieEyesShader->setVec3("u_camPos", ctx.camPosRel);
            m_zombieEyesRender->updateInstances(instances);
            m_zombieEyesRender->draw();
        }
    }

    /*
     * Pneus toxiques : billboard face caméra, mêmes uniformes que les
     * zombies (pas de texture, la forme est procédurale, voir
     * projectile.frag).
     */
    if (m_combat.active() && m_projectilesRender && m_projectilesRender->built()) {
        m_projectileShader->use();
        m_projectileShader->setMat4("u_model", ctx.toRel);
        m_projectileShader->setMat4("u_view", ctx.view);
        m_projectileShader->setMat4("u_proj", ctx.proj);
        m_projectilesRender->updateInstances(m_combat.projectileInstances());
        m_projectilesRender->draw();
    }

    /*
     * Explosions 3D des roquettes : modèle animé émissif joué à chaque impact
     * (voir render::ExplosionFx). Rendu additif dans la passe principale (profondeur
     * lue, pas écrite) pour s'occulter correctement derrière le relief et l'appareil.
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
            m_explosionShader->setMat4("u_view", ctx.view);
            m_explosionShader->setMat4("u_proj", ctx.proj);
            m_explosionShader->setInt("u_texture", 0);
            m_explosionFx->draw(*m_explosionShader, ctx.toRel, instances);
        }
    }
}

void Application::renderHelicopter(const RenderContext& ctx,
                                   const mat4& base,
                                   float rotorAngle,
                                   float rudder,
                                   float cyclicLong,
                                   float cyclicLat,
                                   float collective,
                                   float rotorFraction) {
    /* Hélicoptère : modèle texturé réel s'il est chargé, sinon version dessinée. */
    if (m_loadedHeli) {
        m_modelShader->use();
        m_modelShader->setMat4("u_view", ctx.view);
        m_modelShader->setMat4("u_proj", ctx.proj);
        m_modelShader->setVec3("u_lightDir", ctx.lightDir);
        m_modelShader->setVec3("u_camPos", ctx.camPosRel);
        m_modelShader->setInt("u_texture", 0);
        /* Assiette réelle (roulis, tangage) extraite de l'orientation rendue, pour
           animer l'horizon artificiel du tableau de bord. Axes du corps dans le
           monde : avant = colonne 0, haut = colonne 1, droite = colonne 2. */
        const vec3 fwd = glm::normalize(vec3(base[0]));
        const vec3 upv = glm::normalize(vec3(base[1]));
        const vec3 rgt = glm::normalize(vec3(base[2]));
        const float pitchR = std::asin(clamp(fwd.y, -1.0f, 1.0f));
        const float rollR = std::atan2(-rgt.y, upv.y);
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
        m_loadedHeli->draw(*m_modelShader,
                           ctx.toRel * base,
                           rotorAngle,
                           rotorFraction,
                           m_viewMode != 1,
                           rudder,
                           cyclicLong,
                           cyclicLat,
                           collective,
                           rollR,
                           pitchR,
                           altitudeFt,
                           varioFpm,
                           headingRad,
                           airspeedKt,
                           torquePct);
    } else {
        m_helicopter->draw(*m_shader, ctx.toRel * base, rotorAngle);
    }
}

} /* namespace artouste::app */
