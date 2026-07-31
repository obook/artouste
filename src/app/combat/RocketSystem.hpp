/*
 * RocketSystem.hpp
 * Roquettes tirées par l'appareil (mode zombie) : contrairement à l'ancienne
 * mitrailleuse hitscan, chaque tir lance un projectile qui vole dans l'axe du
 * canon, retombe légèrement (petite gravité), puis explose au sol (ou au
 * contact d'un zombie). L'explosion tue tous les zombies vivants dans un rayon
 * de EXPLOSION_RADIUS_M autour du point d'impact (dégâts de zone), pas
 * seulement celui visé. Le rendu (roquettes en vol, boules de feu) est délégué
 * à ApplicationRenderEffects, qui lit rockets()/explosions().
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/combat/ZombieHorde.hpp"
#include "util/Math.hpp"

#include <functional>
#include <vector>

namespace artouste::app {

class RocketSystem {
public:
    /* Rayon (m) de l'explosion au sol : tout zombie vivant dont le centre est à
       cette distance ou moins du point d'impact est tué. Public : sert aussi de
       rayon visuel de la boule de feu (voir ApplicationRenderEffects). */
    static constexpr float EXPLOSION_RADIUS_M = 3.0f;

    /* Lance une roquette depuis origin (bouche visible du canon) dans la
       direction dir (DOIT être normalisée). Sans effet au-delà d'une capacité
       de sécurité (filet, ne devrait pas arriver à la cadence de tir prévue). */
    void spawn(const vec3& origin, const vec3& dir) noexcept;

    /* Événements d'un pas de temps, pour les sons (voir CombatMode) : la
       logique elle-même ignore l'audio. Les positions sont à grain fin (une
       entrée par ZOMBIE touché/tué, pas par explosion) : une explosion qui
       fauche trois zombies d'un coup doit faire entendre trois cris distincts,
       pas un seul. explosionPositions, elle, reste une entrée par explosion
       (le bruit d'impact est systématique, qu'elle ait ou non touché
       quelqu'un). */
    struct UpdateResult {
        int                explosions = 0;  /* nombre de roquettes ayant explosé ce pas */
        int                kills      = 0;  /* zombies tués par les explosions ce pas */
        std::vector<vec3>  explosionPositions;    /* une entrée par explosion */
        std::vector<vec3>  zombieHitPositions;    /* une entrée par zombie touché sans être tué */
        std::vector<vec3>  zombieDeathPositions;  /* une entrée par zombie tué */
        /* Nombre de zombies tués par CHAQUE explosion (même ordre/longueur que
           explosionPositions) : sert au score (voir CombatMode), qui bonifie
           les kills multiples (plusieurs zombies fauchés par la même
           roquette) plutôt que de simplement compter les morts une à une. */
        std::vector<int>  explosionKillCounts;
    };

    /* Avance chaque roquette (gravité + position), détecte l'impact (sol via
       terrainHeight, ou contact direct d'un zombie sur le segment parcouru),
       déclenche l'explosion et applique les dégâts de zone à horde. Fait aussi
       vieillir les boules de feu affichées. */
    UpdateResult update(float dt, const std::function<float(float, float)>& terrainHeight,
                        ZombieHorde& horde) noexcept;

    /* Ajoute une boule de feu qui ne vient d'aucun tir : le mode zombie s'en sert
       pour faire éclater les marcheurs d'un largueur abattu. Purement visuel --
       ni dégâts de zone, ni trace au sol (ce n'est pas un impact de roquette),
       et l'appelant reste maître des morts et des sons. */
    void addExplosion(const vec3& center);

    /* Roquette en vol, pour le rendu (traînée de feu tendue de tail à head). */
    struct RocketView {
        vec3 head{0.0f};  /* pointe de la roquette (monde) */
        vec3 tail{0.0f};  /* fin de la traînée derrière elle (monde) */
    };
    [[nodiscard]] std::vector<RocketView> rockets() const;

    /* Explosion au sol en cours, pour le rendu (modèle 3D animé, voir
       render::ExplosionFx) : point d'impact et progression 0..1 dans
       l'animation. */
    struct ExplosionView {
        vec3  center{0.0f};
        float progress = 0.0f;  /* 0 = début, 1 = fin de l'animation */
    };
    [[nodiscard]] std::vector<ExplosionView> explosions() const;

    /* Trace de brûlure laissée au sol par chaque impact : point au sol, opacité
       courante (1 -> 0) et forme. Pour un décalque sombre (voir
       Application::drawScorchMarks).

       Deux impacts ne se ressemblent pas. La FORME vient de l'angle d'arrivée :
       une roquette qui tombe à la verticale creuse un rond, une roquette qui
       rase le sol étire sa trace le long de sa trajectoire, comme la tache
       allongée que projette un cône incliné (rapport 1/sin de l'incidence).
       yaw donne la direction de ce grand axe. La TAILLE vient de la portée : un
       tir lointain a plus longtemps accéléré vers le bas et frappe plus fort, on
       lui donne donc une trace plus large, plafonnée. */
    struct ScorchView {
        vec3  center{0.0f};
        float alpha = 0.0f;
        /* Rayon ÉQUIVALENT (m) : l'ellipse conserve cette surface quelle que soit
           sa forme, le grand axe valant radius * racine(elongation) et le petit
           radius / racine(elongation). Sans cette conservation, une trace
           allongée paraîtrait aussi une trace démesurée. */
        float radius     = 0.0f;
        float elongation = 1.0f;  /* rapport grand axe / petit axe (1 = rond) */
        float yaw        = 0.0f;  /* direction du grand axe (rad, repère monde) */
    };
    [[nodiscard]] std::vector<ScorchView> scorches() const;

    /* Vide roquettes, explosions et traces (fin de partie, changement de carte...). */
    void clear() noexcept {
        m_rockets.clear();
        m_explosions.clear();
        m_scorches.clear();
    }

private:
    struct Rocket {
        vec3  position{0.0f};
        vec3  velocity{0.0f};
        vec3  origin{0.0f};      /* point de départ, pour la portée de l'impact (voir ScorchView) */
        float lifetimeS = 0.0f;  /* despawn de sécurité (airburst) si rien touché */
    };
    struct Explosion {
        vec3  center{0.0f};
        float age = 0.0f;  /* s écoulées depuis la détonation */
    };
    struct Scorch {
        vec3  center{0.0f};
        float age        = 0.0f;  /* s écoulées depuis l'impact (s'estompe sur ~45 s) */
        float radius     = 0.0f;  /* figés à la détonation, voir ScorchView */
        float elongation = 1.0f;
        float yaw        = 0.0f;
    };

    std::vector<Rocket>    m_rockets;
    std::vector<Explosion> m_explosions;
    std::vector<Scorch>    m_scorches;
};

}  /* namespace artouste::app */
