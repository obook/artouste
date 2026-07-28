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

    /* Nature d'un zombie. La pondeuse (Brood) est le boss des manches multiples
       de cinq (voir WaveManager) : même modèle et même IA de marche que les
       autres, mais agrandie, très résistante, plus lente, et surtout elle fait
       apparaître des marcheurs autour d'elle tant qu'elle vit -- c'est le
       gestionnaire de vagues qui pilote ces apparitions, la horde ne fait que
       porter l'état. */
    enum class Type { Walker, Brood };

    /* Vie d'une pondeuse : la roquette inflige 1000 de dégâts de zone
       (RocketSystem::BLAST_DAMAGE), il faut donc cinq impacts pour l'abattre,
       là où un marcheur (100 PV) tombe du premier. */
    static constexpr float BROOD_HEALTH = 5000.0f;
    /* Facteur d'échelle du modèle et de sa sphère de collision : une silhouette
       qu'on repère de loin (près de six mètres de haut, le modèle étant
       normalisé à 1,80 m), et une cible plus facile à toucher en compensation du
       nombre de roquettes nécessaires. */
    static constexpr float BROOD_SCALE = 3.2f;
    /* La pondeuse avance nettement moins vite qu'un marcheur : sa menace est ce
       qu'elle engendre, pas sa course. */
    static constexpr float BROOD_SPEED_FACTOR = 0.45f;

    struct Zombie {
        vec3  position{0.0f};        /* centre au sol (monde) */
        float yaw           = 0.0f;  /* orientation (rad), pour varier les silhouettes */
        float phase         = 0.0f;  /* déphasage de l'oscillation d'attente, par zombie */
        float health        = 100.0f;
        float throwCooldownS = 0.0f;  /* utilisé à partir de l'étape 3 (boulettes toxiques) */
        State state         = State::Alive;
        float stateTimer    = 0.0f;   /* durée avant despawn une fois Dying */
        float hitFlashTimer = 0.0f;   /* décompte du flash de coup touché (voir applyDamage) */
        Type  type          = Type::Walker;
        /* Échelle du modèle ET de la sphère de collision (voir RocketSystem) :
           1 pour un marcheur, BROOD_SCALE pour une pondeuse. Un seul champ pour
           les deux, sans quoi la silhouette et la cible finiraient par diverger. */
        float scale         = 1.0f;
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

    /* Fait apparaître une pondeuse (le boss, voir Type) à la position donnée.
       Une seule à la fois en pratique : le gestionnaire de vagues n'en lance
       qu'une par manche de boss, et les accesseurs ci-dessous renvoient la
       première trouvée. */
    void spawnBrood(const vec3& position, float yaw = 0.0f, float phase = 0.0f);

    /* Une pondeuse est-elle encore debout (Alive, ni en train de tomber ni
       morte) ? Le gestionnaire de vagues s'en sert pour engendrer autour d'elle
       et pour retenir la fin de manche ; le HUD, pour afficher sa jauge. */
    [[nodiscard]] bool broodAlive() const noexcept;
    /* Position de cette pondeuse, ou l'origine s'il n'y en a plus (à lire
       seulement quand broodAlive() est vrai). */
    [[nodiscard]] vec3 broodPosition() const noexcept;
    /* Vie restante de cette pondeuse (0..1), 0 s'il n'y en a plus. */
    [[nodiscard]] float broodHealthPct() const noexcept;

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

    /* Une lueur d'oeil à dessiner (voir render::combat::ZombieEyes) : deux par
       zombie affiché. Type volontairement neutre (pas de type du module de
       rendu ici), converti à l'image par l'adaptateur de rendu -- même principe
       que RocketSystem::ExplosionView. */
    struct EyeView {
        vec3  position{0.0f};  /* repère monde */
        float radius = 0.0f;   /* rayon de la lueur (m), avant grossissement à distance */
        vec3  color{0.0f};     /* couleur ET intensité (elle s'éteint à la mort) */
    };

    /* Deux lueurs par zombie encore affiché, calées sur la même matrice
       d'instance que buildInstanceMatrices (donc soumises au même balancement
       et à la même bascule de chute). Elles sont posées sur la tête du modèle
       en dur, sans suivre l'os du cou animé par le squelette : à la distance de
       jeu (vue d'hélicoptère), l'écart ne se voit pas, et cela évite de calculer
       une pose par instance. Verte pour un marcheur, rouge pour une pondeuse. */
    [[nodiscard]] std::vector<EyeView> buildEyes() const;

    [[nodiscard]] std::size_t count() const noexcept { return m_zombies.size(); }
    [[nodiscard]] std::vector<Zombie>&       zombies() noexcept { return m_zombies; }
    [[nodiscard]] const std::vector<Zombie>& zombies() const noexcept { return m_zombies; }

private:
    /* Matrice monde d'un zombie (translation, balancement d'attente ou bascule
       de chute, orientation, échelle) : source unique de buildInstanceMatrices
       et de buildEyes, pour que les lueurs ne puissent pas se décaler du corps. */
    [[nodiscard]] mat4 instanceMatrix(const Zombie& z) const;

    std::vector<Zombie> m_zombies;
    float                m_time = 0.0f;  /* horloge locale de la horde (oscillation d'attente) */
    /* Cooldowns de jet désynchronisés entre zombies (voir update) : RNG dédié
       plutôt qu'un tirage global, pour rester déterministe par horde. */
    std::mt19937         m_rng{std::random_device{}()};
};

}  /* namespace artouste::app */
