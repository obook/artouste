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

    /* Trace de brûlure laissée au sol par chaque impact, s'estompant en ~45 s :
       point au sol et opacité courante (1 -> 0). Pour un décalque sombre (voir
       Application::drawScorchMarks). */
    struct ScorchView {
        vec3  center{0.0f};
        float alpha = 0.0f;
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
        float lifetimeS = 0.0f;  /* despawn de sécurité (airburst) si rien touché */
    };
    struct Explosion {
        vec3  center{0.0f};
        float age = 0.0f;  /* s écoulées depuis la détonation */
    };
    struct Scorch {
        vec3  center{0.0f};
        float age = 0.0f;  /* s écoulées depuis l'impact (s'estompe sur ~45 s) */
    };

    std::vector<Rocket>    m_rockets;
    std::vector<Explosion> m_explosions;
    std::vector<Scorch>    m_scorches;
};

}  /* namespace artouste::app */
