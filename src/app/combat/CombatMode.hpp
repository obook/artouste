/*
 * CombatMode.hpp
 * Mode zombie, orthogonal à la boucle de vol (le joueur garde la main sur le
 * pilotage) : même contrat start()/stop()/active()/update() que
 * LandingAutopilot, mais tourne EN PARALLÈLE de computeControls plutôt que de
 * piloter l'appareil. Délègue à WaveManager le chargement des points de spawn
 * de la carte courante (zombies.txt, présence = carte compatible mode
 * zombie) et le pilotage des vagues. Orchestre à chaque image : tir (Weapon)
 * -> vagues (WaveManager, décide des spawns) -> déplacement et jets de la
 * horde (ZombieHorde) -> boulettes toxiques (ProjectileSystem) -> dégâts au
 * joueur.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/combat/ProjectileSystem.hpp"
#include "app/combat/RocketSystem.hpp"
#include "app/combat/WaveManager.hpp"
#include "app/combat/Weapon.hpp"
#include "app/combat/ZombieHorde.hpp"
#include "physics/RigidBody.hpp"
#include "util/Math.hpp"

#include <filesystem>
#include <functional>
#include <vector>

namespace artouste::app {

class CombatMode {
public:
    /* Délègue à WaveManager le chargement de <terrainDir>/zombies.txt et le
       peuplement de la première vague, calée d'emblée sur le relief
       (terrainHeight) : sans quoi un zombie resterait un instant à
       l'altitude 0 (sous le sol dès que le relief local est notablement
       au-dessus du niveau de la mer) avant la première image de mise à jour.
       Réinitialise la vie du joueur, l'état de fin de partie et le chrono de
       session. Sans effet (active() reste faux) si le fichier est absent ou
       vide : la carte n'est simplement pas compatible avec le mode zombie. */
    void start(const std::filesystem::path& terrainDir,
              const std::function<float(float, float)>& terrainHeight) noexcept;

    /* Arrête le mode et vide la horde (retour au menu, changement de carte
       incompatible...). */
    void stop() noexcept;

    [[nodiscard]] bool active() const noexcept { return m_active; }

    /* Avance le mode d'un pas de temps : tire (Weapon) si fireTrigger est
       tenu, depuis le canon de l'appareil (position/cap de body) ; avance le
       gestionnaire de vagues (WaveManager, spawn échelonné et difficulté
       croissante) ; fait avancer la horde (marche vers le joueur, jets de
       boulettes toxiques selon l'altitude/portée) ; avance les boulettes en
       vol et applique leurs dégâts au joueur, jusqu'à déclencher gameOver()
       sous 0 PV. Sans effet si le mode n'est pas actif, ou déjà en fin de
       partie (gameOver). */
    void update(float dt, const physics::RigidBody& body, bool fireTrigger,
               const std::function<float(float, float)>& terrainHeight) noexcept;

    /* Matrices de transformation courantes (repère monde), prêtes pour
       render::combat::SkinnedZombies::updateInstances. */
    [[nodiscard]] std::vector<mat4> zombieTransforms() const {
        return m_horde.buildInstanceMatrices();
    }

    /* Flashs de coup touché courants, même ordre que zombieTransforms(). */
    [[nodiscard]] std::vector<float> zombieHitFlashes() const {
        return m_horde.buildHitFlashes();
    }

    /* Positions courantes (repère monde) des zombies encore affichés -- pour
       un repérage annexe (minimap), voir ApplicationHudNav.cpp. */
    [[nodiscard]] std::vector<vec3> zombiePositions() const { return m_horde.alivePositions(); }

    /* "kind" de chaque zombie affiché (même ordre que zombieTransforms) : le
       rendu skinné y lit variante de personnage et groupe de phase de marche. */
    [[nodiscard]] std::vector<int> zombieKinds() const { return m_horde.buildKinds(); }

    /* Instances de boulettes toxiques courantes, prêtes pour
       render::Projectiles::updateInstances. */
    [[nodiscard]] std::vector<vec4> projectileInstances() const {
        return m_projectiles.buildInstances();
    }

    /* Munitions restantes / recharge en cours (pour le HUD, étape 5). */
    [[nodiscard]] int  ammo() const noexcept { return m_weapon.ammo(); }
    [[nodiscard]] int  ammoMax() const noexcept { return Weapon::AMMO_MAX; }
    [[nodiscard]] bool reloading() const noexcept { return m_weapon.reloading(); }

    /* Le joueur est-il actuellement sous le plafond d'altitude des boulettes
       toxiques (donc vulnérable) ? Reflète la dernière image mise à jour, pas
       une lecture instantanée. Pour le HUD (indicateur de danger). */
    [[nodiscard]] bool belowCeiling() const noexcept {
        return m_lastPlayerAgl <= ZombieHorde::TOXIC_CEILING_M;
    }

    /* Vie du joueur (0..1) et fin de partie (0 PV). */
    [[nodiscard]] float healthPct() const noexcept { return m_playerHealth / PLAYER_HEALTH_MAX; }
    [[nodiscard]] bool  gameOver() const noexcept { return m_gameOver; }

    /* Largueur (boss des manches multiples de cinq, voir WaveManager) : présence
       et vie restante (0..1), pour la jauge du HUD. */
    [[nodiscard]] bool  broodActive() const noexcept { return m_horde.broodAlive(); }
    [[nodiscard]] float broodHealthPct() const noexcept { return m_horde.broodHealthPct(); }

    /* Couleur et taille des lueurs d'yeux de chaque zombie affiché (leur
       position vient du rendu, qui seul connaît la pose de la tête). */
    [[nodiscard]] std::vector<ZombieHorde::EyeTint> zombieEyeTints() const {
        return m_horde.buildEyeTints();
    }

    /* Vague en cours et durée totale de la session -- pour le HUD (étape 5). */
    [[nodiscard]] int   wave() const noexcept { return m_waves.waveNumber(); }
    [[nodiscard]] float elapsedS() const noexcept { return m_elapsedS; }

    /* Score en points : 25 par zombie tué, mais bonifié par explosion selon le
       nombre de victimes fauchées d'un coup (kill multiple) -- 75 pour un
       double kill, 125 pour un triple kill ou plus -- plutôt que la simple
       somme de kills individuels (voir killScoreForCount, CombatMode.cpp). */
    [[nodiscard]] int   score() const noexcept { return m_score; }

    /* Nombre total de zombies tués depuis le début de la session (par les
       explosions de roquettes) : compteur distinct du score en points, pour le
       HUD. */
    [[nodiscard]] int   kills() const noexcept { return m_kills; }

    /* Événements survenus lors du dernier update() : la logique de jeu
       (Weapon, ZombieHorde, ProjectileSystem, WaveManager) ignore l'audio ;
       c'est l'appelant (Application, voir ApplicationLoop.cpp) qui détecte
       ces événements pour déclencher les sons, même principe que le son de
       démarrage turbine (comparaison d'état d'une image à l'autre). */
    struct SoundEvents {
        bool fired = false;  /* un coup de mitrailleuse est parti, depuis muzzlePos */
        vec3 muzzlePos{0.0f};

        /* Une entrée par explosion de roquette survenue ce pas, à sa position
           réelle (pour le volume selon la distance à l'hélico, voir
           AudioEngine::playExplosion etc.) : le bruit d'explosion accompagne
           toujours l'impact, et en plus un cri de zombie touché ou tué selon
           qu'elle a fait au moins une victime (mutuellement exclusifs, voir
           RocketSystem::ExplosionEvent). */
        std::vector<vec3> explosionPositions;
        std::vector<vec3> zombieHitPositions;
        std::vector<vec3> zombieDeathPositions;

        /* Une entrée par boulette toxique lancée ce pas, à son origine (bras du
           zombie lanceur). */
        std::vector<vec3> throwPositions;

        /* L'appareil a encaissé un coup, boulette toxique ou contact avec le sol
           (voir applyGroundImpact) : même bruit, joué à sa position, donc à
           distance nulle. */
        bool impacted = false;

        /* Nouvelle vague et apparition d'un largueur : sons non spatiaux, à
           volume fixe -- seules exceptions au principe "volume selon la distance
           à l'hélico" (voir AudioEngine::playWaveStart et playBroodSpawn). Une
           manche de boss lève les deux le même pas : l'annonce de vague, puis le
           râle par-dessus. */
        bool waveStart   = false;
        bool broodSpawned = false;
    };
    [[nodiscard]] const SoundEvents& soundEvents() const noexcept { return m_events; }

    /* Annonce affichée au HUD quand une même explosion fauche plusieurs zombies
       d'un coup (voir killScoreForCount, même seuils que le score), ou quand la
       pondeuse tombe (Brood, qui prime sur un kill multiple simultané) : reste
       affichée KILL_ANNOUNCE_DURATION_S après l'événement qui l'a déclenchée,
       puis retombe à None. */
    enum class KillAnnouncement { None, Double, Triple, Carnage, Brood };
    [[nodiscard]] KillAnnouncement killAnnouncement() const noexcept {
        return m_killAnnounceTimer > 0.0f ? m_killAnnounce : KillAnnouncement::None;
    }

    /* Flash de bouche courant (retour visuel du tir, indépendant du son -- voir
       ApplicationRenderEffects.cpp) : actif quelques dizaines de ms après
       chaque coup parti, pour rester visible même en rafale rapide. Position
       et direction du canon à cet instant, pour placer le flash devant
       l'appareil plutôt qu'en son centre. */
    [[nodiscard]] bool muzzleFlashActive() const noexcept { return m_muzzleFlashTimer > 0.0f; }
    [[nodiscard]] const vec3& muzzleWorldDir() const noexcept { return m_lastFireDir; }

    /* Position monde du canon visible (bouche apparente), en avant et un peu
       au-dessus du centre de l'appareil : origine commune du flash de bouche et
       des roquettes, pour qu'ils partent du même point devant le cockpit (voir
       ApplicationRenderEffects.cpp). */
    [[nodiscard]] vec3 muzzleVisualPos() const noexcept {
        return m_lastMuzzlePos + m_lastFireDir * MUZZLE_FWD_M + vec3{0.0f, MUZZLE_UP_M, 0.0f};
    }

    /* Roquettes en vol et explosions au sol en cours, prêtes pour le rendu
       (ApplicationRenderEffects : traînée de feu et boules de feu). */
    [[nodiscard]] std::vector<RocketSystem::RocketView>    rockets() const { return m_rockets.rockets(); }
    [[nodiscard]] std::vector<RocketSystem::ExplosionView> explosions() const {
        return m_rockets.explosions();
    }
    /* Traces de brûlure au sol laissées par les impacts (décalques sombres qui
       s'estompent, voir Application::drawScorchMarks). */
    [[nodiscard]] std::vector<RocketSystem::ScorchView> scorches() const {
        return m_rockets.scorches();
    }

    /* Contact avec le sol, à la vitesse d'arrivée mesurée par la physique (voir
       physics::FlightModel::consumeGroundImpact) : au-delà de la vitesse tolérée,
       l'appareil encaisse des dégâts proportionnels à l'excès et fait le même
       bruit qu'une boulette reçue. Sans effet hors combat, partie perdue, ou sous
       le seuil : un posé normal ne coûte rien. À appeler après update(), qui
       remet les événements sonores à zéro. */
    void applyGroundImpact(float speedMs);

private:
    static constexpr float PLAYER_HEALTH_MAX = 100.0f;
    /* Vitesse d'arrivée (m/s) en deçà de laquelle le contact est un posé et non
       un choc, puis dégâts par (m/s) d'excès AU CARRÉ. Le carré plutôt qu'une
       droite : les dégâts suivent alors l'énergie du choc, si bien qu'une touche
       un peu ferme ne coûte presque rien (2 points à 5 m/s, 9 à 8 m/s) alors
       qu'un vrai crash reste fatal (100 points à 20 m/s). Une droite faisait
       perdre trop de vie à chaque contact. */
    static constexpr float GROUND_IMPACT_FREE_MS      = 3.0f;
    static constexpr float GROUND_IMPACT_DAMAGE_COEFF = 0.35f;
    /* Décalage du canon visible par rapport au centre de l'appareil : en avant
       de l'oeil du pilote (COCKPIT_EYE.x ~3,55 m) pour rester devant lui en vue
       cockpit, et légèrement remonté. */
    static constexpr float MUZZLE_FWD_M = 6.0f;
    static constexpr float MUZZLE_UP_M  = 1.0f;

    /* Points accordés en plus pour l'abattage d'un largueur, au-delà des points
       de l'explosion qui l'a achevé : de l'ordre de vingt marcheurs, à la
       mesure des cinq roquettes qu'il encaisse. */
    static constexpr int BROOD_SCORE = 500;
    /* Points par marcheur éclaté avec le largueur, comptés UN PAR UN et non au
       barème du kill multiple : celui-ci plafonne à trois têtes, ce qui convient
       au souffle d'une roquette mais pas ici, où le largueur peut en avoir lâché
       quinze. Même valeur qu'un marcheur tué seul. */
    static constexpr int BROODLING_SCORE = 25;

    bool             m_active        = false;
    bool             m_gameOver      = false;
    /* Largueur debout à la fin de l'update précédent : sa disparition d'une
       image à l'autre vaut mise à mort (même principe de comparaison d'état que
       les événements sonores). */
    bool             m_broodWasAlive = false;
    /* Force l'annonce sonore (waveStart) au tout premier update() suivant
       start() : la manche 1 est peuplée par start() lui-même, avant le
       premier update(), donc la comparaison de numéro de manche habituelle ne
       la détecterait jamais (voir CombatMode::start/update). */
    bool             m_firstWavePending = false;
    float            m_playerHealth  = PLAYER_HEALTH_MAX;
    float            m_elapsedS      = 0.0f;  /* durée totale de la session en cours */
    int              m_kills         = 0;     /* zombies tués depuis le début de la session */
    int              m_score         = 0;     /* points, voir score() */
    float            m_lastPlayerAgl = 0.0f;  /* hauteur sol de la dernière image (voir belowCeiling) */
    float            m_muzzleFlashTimer = 0.0f;  /* s restantes d'affichage du flash de bouche */
    KillAnnouncement m_killAnnounce      = KillAnnouncement::None;
    float            m_killAnnounceTimer = 0.0f;  /* s restantes d'affichage de l'annonce */
    vec3             m_lastMuzzlePos{0.0f};
    vec3             m_lastFireDir{1.0f, 0.0f, 0.0f};
    SoundEvents      m_events;                /* réinitialisés à chaque update() */
    ZombieHorde      m_horde;
    Weapon           m_weapon;
    RocketSystem     m_rockets;
    ProjectileSystem m_projectiles;
    WaveManager      m_waves;
};

}  /* namespace artouste::app */
