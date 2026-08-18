/*
 * RocketSystemReglages.hpp
 * Réglages des roquettes, partagés par la simulation et les vues du rendu.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::app {

/* Capacité de sécurité : au-delà, on ignore les nouveaux tirs plutôt que de
   laisser le nombre de roquettes en vol dériver sans borne. */
constexpr std::size_t MAX_ROCKETS = 64;

/* Vitesse de la roquette (m/s) : rapide, mais assez lente pour se voir voler
   du canon jusqu'au sol. */
constexpr float ROCKET_SPEED_MS = 130.0f;
/* Gravité appliquée à la roquette (m/s^2) : faible devant la vitesse, juste de
   quoi la faire retomber et exploser au sol même tirée à peu près à plat. */
constexpr float ROCKET_GRAVITY = 12.0f;
/* Despawn de sécurité (s) si la roquette ne touche ni le sol ni un zombie
   (tir vers le ciel) : elle finit par exploser en l'air, sans dégâts utiles. */
constexpr float ROCKET_LIFETIME_S = 5.0f;
/* Longueur de la traînée de feu affichée derrière la roquette (m). */
constexpr float ROCKET_TRAIL_M = 5.0f;

/* Rayon de contact direct roquette/zombie en vol (m) : une roquette qui frôle
   un zombie explose sur lui plutôt que de le traverser. */
constexpr float DIRECT_HIT_RADIUS_M = 1.2f;
/* Hauteur du centre du zombie visé pour ce test de contact (m au-dessus du
   sol), même logique que la sphère de collision de la mitrailleuse d'avant. */
constexpr float DIRECT_HIT_HEIGHT_M = 0.9f;
/* Dégâts appliqués à chaque zombie dans le rayon d'explosion : très au-delà de
   la vie (100) pour une mise à mort certaine en zone. */
constexpr float BLAST_DAMAGE = 1000.0f;

/* Durée d'affichage de l'explosion (s) : progress parcourt 0->1 sur cette durée.
   DOIT valoir la tranche d'animation jouée par render::ExplosionFx (playSpanS,
   voir ApplicationScene) pour une lecture a vitesse réelle du flipbook -- sinon
   les images discretes du pack "sautent". On ne joue qu'une partie de l'anim
   (3 s au total) pour un impact punchy. */
constexpr float EXPLOSION_DURATION_S = 1.2f;

/* Durée de vie d'une trace de brûlure au sol (s) : s'estompe progressivement. */
constexpr float SCORCH_DURATION_S = 300.0f;  /* 5 minutes */

/* Forme et taille de la trace au sol (voir RocketSystem::ScorchView).

   Taille : rayon de base à bout portant, puis élargissement avec la portée du
   tir (une roquette partie de loin arrive plus vite et plus bas), plafonné pour
   qu'un tir à travers toute l'arène ne laisse pas un cratère absurde. Ces
   valeurs ne changent RIEN à la zone létale, qui reste EXPLOSION_RADIUS_M : la
   trace est un décalque, pas une hitbox. */
constexpr float SCORCH_BASE_RADIUS_M = 3.0f;
constexpr float SCORCH_MAX_RADIUS_M  = 5.0f;
constexpr float SCORCH_RANGE_REF_M   = 150.0f;  /* portée à laquelle le gain est atteint */
constexpr float SCORCH_RANGE_GAIN    = 0.5f;    /* +50 % de rayon à cette portée */

/* Forme : rapport grand axe / petit axe. Un cône qui frappe le sol sous
   l'incidence i y projette une tache de rapport 1/sin(i), mais cette loi diverge
   à l'horizontale : appliquée telle quelle, elle saturerait son plafond pour
   presque tous les tirs réels (canon fixe, nez à peine piqué, donc incidence
   d'arrivée souvent sous 20 degrés) et toutes les traces se ressembleraient. On
   garde donc la même tendance -- plus l'arrivée est rasante, plus la tache
   s'allonge -- sur une interpolation bornée, qui étale les cas de jeu entre le
   rond et l'allongement maximal. */
constexpr float SCORCH_ELONGATION_GAIN = 1.6f;  /* allongement maximal : 1 + ce gain */
/* Filet de sécurité : au-delà, on retire la plus ancienne trace. Un tir
   quasi continu pendant les 5 minutes de vie d'une trace produirait bien plus
   d'impacts que ce plafond (des centaines) ; dans ce cas les plus anciennes
   disparaissent avant terme plutôt que de multiplier les décalques à l'écran. */
constexpr std::size_t MAX_SCORCHES = 400;

/* Forme et taille de la trace laissée par une roquette qui vient d'exploser :
   'velocity' est sa vitesse à la détonation, 'rangeM' la distance horizontale
   parcourue depuis le canon. Voir RocketSystem::ScorchView pour le raisonnement.
   Une roquette sans vitesse exploitable (cas dégénéré) laisse une trace ronde. */
struct ScorchShape {
    float radius;
    float elongation;
    float yaw;
};

inline ScorchShape scorchShapeFor(const vec3& velocity, float rangeM) noexcept {
    const float speed = glm::length(velocity);
    if (speed < 1e-4f) {
        return ScorchShape{SCORCH_BASE_RADIUS_M, 1.0f, 0.0f};
    }
    const vec3 dir = velocity / speed;

    /* Sinus de l'incidence : 1 pour une chute verticale, 0 en rasant le sol. */
    const float sinIncidence = saturate(-dir.y);
    const float elongation   = 1.0f + SCORCH_ELONGATION_GAIN * (1.0f - sinIncidence);

    const float growth = 1.0f + SCORCH_RANGE_GAIN * std::min(1.0f, rangeM / SCORCH_RANGE_REF_M);
    const float radius = std::min(SCORCH_MAX_RADIUS_M, SCORCH_BASE_RADIUS_M * growth);

    /* Direction horizontale du tir : grand axe de la tache. Une roquette
       parfaitement verticale n'a pas de direction au sol, mais son élongation
       vaut alors 1 et l'orientation n'a plus d'effet visible. */
    const float yaw = std::atan2(dir.x, dir.z);

    return ScorchShape{radius, elongation, yaw};
}

} /* namespace artouste::app */
