/*
 * ZombieHorde.hpp
 * État CPU de la horde de zombies : une position, une vie et un état par
 * instance, plus l'IA de déplacement vers le joueur (marche en ligne droite,
 * recalée sur le relief) et les jets de boulettes toxiques. Traduit cet état
 * en matrices de transformation prêtes pour le rendu instancié skinné (voir
 * render::SkinnedZombies), qui anime la marche et les bras côté GPU.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <cstddef>
#include <functional>
#include <random>
#include <vector>

namespace artouste::app {

/* Demande de jet d'une boulette toxique, produite par ZombieHorde::update()
   pour chaque zombie à portée, hors cooldown, avec le joueur sous le plafond
   d'altitude -- CombatMode la transmet à ProjectileSystem::spawn. */
struct ThrowRequest {
    vec3 origin;  /* position de départ de la boulette (à hauteur de bras du zombie) */
    vec3 target;  /* position du joueur au moment du jet (pas de guidage ensuite) */
};

class ZombieHorde {
public:
    /* Plafond d'altitude (m, hauteur sol) sous lequel les zombies peuvent
       viser l'appareil : voler au-dessus met hors de portée, quelle que soit
       la distance horizontale. Public : exposé au HUD (indicateur "sous le
       plafond", voir CombatMode::belowCeiling). */
    static constexpr float TOXIC_CEILING_M = 35.0f;

    enum class State { Alive, Dying, Dead };

    struct Zombie {
        vec3  position{0.0f};        /* centre au sol (monde) */
        float yaw           = 0.0f;  /* orientation (rad), pour varier les silhouettes */
        float phase         = 0.0f;  /* déphasage de l'oscillation d'attente, par zombie */
        float health        = 100.0f;
        float throwCooldownS = 0.0f;  /* utilisé à partir de l'étape 3 (boulettes toxiques) */
        State state         = State::Alive;
        float stateTimer    = 0.0f;   /* durée avant despawn une fois Dying */
        float hitFlashTimer = 0.0f;   /* décompte du flash de coup touché (voir applyDamage) */
        /* Entier tiré à l'apparition : le rendu skinné en déduit, de façon
           stable, la variante de personnage et le groupe de phase de marche
           (voir render::SkinnedZombies). La horde reste ignorante de leur
           nombre. */
        unsigned int kind = 0;
    };

    /* Fait apparaître un zombie à la position donnée. yaw/phase varient
       l'orientation et le déphasage de l'oscillation d'attente entre zombies
       (sans quoi ils seraient tous parfaitement synchronisés). */
    void spawn(const vec3& position, float yaw = 0.0f, float phase = 0.0f);

    /* Vide la horde (fin de partie, changement de carte...). */
    void clear() noexcept { m_zombies.clear(); }

    /* Inflige des dégâts au zombie d'indice donné (voir zombies(), indexé
       comme le vecteur qu'elle expose) : déclenche le flash de coup touché,
       et sous 0 PV fait passer Alive -> Dying (anim de chute, despawn après
       DEATH_ANIM_DURATION_S). Sans effet si l'indice est hors bornes ou si le
       zombie n'est pas Alive (déjà en train de tomber ou mort). */
    void applyDamage(std::size_t index, float amount) noexcept;

    /* Recale l'altitude de chaque zombie sur le relief, sans avancer l'IA ni
       les cooldowns : utilisé une fois au spawn (CombatMode::start), avant la
       première image de jeu, où aucune position de joueur n'est encore
       pertinente. */
    void snapToGround(const std::function<float(float, float)>& terrainHeight) noexcept;

    /* Avance chaque zombie vivant d'un pas de temps : le fait marcher en
       ligne droite vers playerPos (plan horizontal, vitesse WALK_SPEED_MS *
       speedFactor -- voir WaveManager pour l'escalade de difficulté), recale
       l'altitude sur le relief (terrainHeight), avance l'anim de chute et le
       flash de coup touché, retire les zombies dont l'anim de chute est
       terminée. playerAgl (hauteur du joueur au-dessus du sol, m) détermine
       si les zombies à portée et hors cooldown peuvent lancer une boulette
       toxique (sous TOXIC_CEILING_M) ; renvoie leurs demandes de jet. */
    std::vector<ThrowRequest> update(float dt, const vec3& playerPos, float playerAgl,
                                     float speedFactor,
                                     const std::function<float(float, float)>& terrainHeight) noexcept;

    /* Matrices de transformation courantes (repère monde ; un zombie vivant ou
       en train de tomber = une instance, les morts ne sont plus dessinés),
       prêtes pour render::SkinnedZombies::updateInstances. */
    [[nodiscard]] std::vector<mat4> buildInstanceMatrices() const;

    /* Intensité du flash de coup touché (0 = normal, 1 = vient d'être touché),
       un élément par instance, dans le MÊME ordre/filtrage que
       buildInstanceMatrices (les deux vecteurs s'indexent pareil). */
    [[nodiscard]] std::vector<float> buildHitFlashes() const;

    /* Positions courantes (repère monde) des zombies encore affichés (vivants
       ou en train de tomber, même filtrage que buildInstanceMatrices) : pour
       un repérage annexe (minimap) qui n'a pas besoin de l'orientation. */
    [[nodiscard]] std::vector<vec3> alivePositions() const;

    /* "kind" de chaque zombie encore affiché, MÊME ordre/filtrage que
       buildInstanceMatrices/buildHitFlashes : le rendu skinné y lit la variante
       et le groupe de phase de chaque instance. */
    [[nodiscard]] std::vector<int> buildKinds() const;

    [[nodiscard]] std::size_t count() const noexcept { return m_zombies.size(); }
    [[nodiscard]] std::vector<Zombie>&       zombies() noexcept { return m_zombies; }
    [[nodiscard]] const std::vector<Zombie>& zombies() const noexcept { return m_zombies; }

private:
    std::vector<Zombie> m_zombies;
    float                m_time = 0.0f;  /* horloge locale de la horde (oscillation d'attente) */
    /* Cooldowns de jet désynchronisés entre zombies (voir update) : RNG dédié
       plutôt qu'un tirage global, pour rester déterministe par horde. */
    std::mt19937         m_rng{std::random_device{}()};
};

}  /* namespace artouste::app */
