/*
 * ApplicationRenderActors.cpp
 * Rendu des entités dynamiques d'une image : mode zombie (personnages
 * skinnés, boulettes toxiques, explosions 3D des roquettes) et l'hélicoptère
 * (modèle FlightGear texturé ou repli procédural). Le décor statique est dans
 * ApplicationRenderWorld.cpp ; l'orchestration dans ApplicationRender.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/AppConstants.hpp"
#include "app/Application.hpp"
#include "render/HelicopterModel.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Shader.hpp"
#include "render/combat/ExplosionFx.hpp"
#include "render/combat/Projectiles.hpp"
#include "render/combat/SkinnedZombies.hpp"
#include "render/combat/ZombieEyes.hpp"
#include "util/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace artouste::app {

void Application::renderCombatEntities(const RenderContext& ctx, float timeSeconds) {
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
     * Boulettes toxiques : billboard face caméra, mêmes uniformes que les
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

        /* (Un disque rotor translucide remplaçant les pales distinctes à haut régime,
           pour éviter l'effet stroboscopique, reste à étudier ; voir l'historique
           git pour une ébauche.) */
    } else {
        m_helicopter->draw(*m_shader, ctx.toRel * base, rotorAngle);
    }
}

} /* namespace artouste::app */
