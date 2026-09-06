/*
 * CombatMode.hpp
 * Mode zombie, orthogonal à la boucle de vol (le joueur garde la main sur le
 * pilotage) : même contrat start()/stop()/active()/update() que
 * LandingAutopilot, mais tourne EN PARALLÈLE de computeControls plutôt que de
 * piloter l'appareil. Délègue à WaveManager le chargement des points de spawn
 * de la carte courante (zombies.txt, présence = carte compatible mode
 * zombie) et le pilotage des vagues. Orchestre à chaque image : tir (Weapon)
 * -> vagues (WaveManager, décide des spawns) -> déplacement et jets de la
 * horde (ZombieHorde) -> pneus toxiques (ProjectileSystem) -> dégâts au
 * joueur.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/combat/BonusSphereReglages.hpp"
#include "app/combat/ProjectileSystem.hpp"
#include "app/combat/RocketSystem.hpp"
#include "app/combat/WaveManager.hpp"
#include "app/combat/Weapon.hpp"
#include "app/combat/ZombieHorde.hpp"
#include "physics/constants.hpp"
#include "physics/RigidBody.hpp"
#include "util/Math.hpp"

#include <filesystem>
#include <functional>
#include <random>
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
       pneus toxiques selon l'altitude/portée) ; avance les pneus en
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

    /* Instances de pneus toxiques courants, prêts pour
       render::Projectiles::updateInstances. */
    [[nodiscard]] std::vector<vec4> projectileInstances() const {
        return m_projectiles.buildInstances();
    }

    /* Munitions restantes / recharge en cours (pour le HUD, étape 5). */
    [[nodiscard]] int  ammo() const noexcept { return m_weapon.ammo(); }
    [[nodiscard]] int  ammoMax() const noexcept { return Weapon::AMMO_MAX; }
    [[nodiscard]] bool reloading() const noexcept { return m_weapon.reloading(); }

    /* Le joueur est-il actuellement sous le plafond d'altitude des pneus
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
        /* Les morts sont réparties en deux listes selon le sort de la fusée de
           bonus tirée par leur explosion (voir chanceFuseeBonus) : le joueur
           entend au cri s'il a gagné quelque chose, sans quitter des yeux ce
           qu'il pilote. Un zombie qui meurt autrement qu'en explosion de
           roquette (marcheurs d'un largueur abattu, sphère noire) va toujours
           en Simple : aucune fusée n'en part. */
        std::vector<vec3> zombieDeathSimplePositions;
        std::vector<vec3> zombieDeathBonusPositions;

        /* Une entrée par pneu toxique lancé ce pas, à son origine (bras du
           zombie lanceur). */
        std::vector<vec3> throwPositions;

        /* L'appareil a encaissé un coup, pneu toxique ou contact avec le sol
           (voir applyGroundImpact) : même bruit, joué à sa position, donc à
           distance nulle. */
        bool impacted = false;

        /* Nouvelle vague et apparition d'un largueur : sons non spatiaux, à
           volume fixe -- seules exceptions au principe "volume selon la distance
           à l'hélico" (voir AudioEngine::playWaveStart et playRale). Une
           manche de boss lève les deux le même pas : l'annonce de vague, puis le
           râle par-dessus. */
        bool waveStart   = false;
        bool broodSpawned = false;
        /* Largueur abattu : même râle qu'à son arrivée, pour clore
           l'affrontement par le cri qui l'avait ouvert. */
        bool broodKilled  = false;

        /* Volume traversé ce pas, joué à la position de l'appareil comme un
           impact (distance nulle). */
        bool bonusPickup = false;

        /* Une entrée par chandelle partie ce pas (kill payant d'une sphère),
           au point d'explosion d'où elle décolle. */
        std::vector<vec3> bonusLaunchPositions;

        /* Une entrée par sphère qui commence à s'ouvrir ce pas, à l'altitude où
           elle apparaît (bout de la chandelle) : listes séparées pour les sphères
           de vie et de mort, qui s'annoncent avec leur propre son. */
        std::vector<vec3> bonusOpenPositions;
        std::vector<vec3> bonusOpenSantePositions;
        std::vector<vec3> bonusOpenMortPositions;
    };
    [[nodiscard]] const SoundEvents& soundEvents() const noexcept { return m_events; }

    /* Annonce affichée au HUD quand une même explosion fauche plusieurs zombies
       d'un coup (voir killScoreForCount, même seuils que le score), ou quand la
       le largueur tombe (Brood, qui prime sur un kill multiple simultané) : reste
       affichée KILL_ANNOUNCE_DURATION_S après l'événement qui l'a déclenchée,
       puis retombe à None. */
    enum class KillAnnouncement { None, Double, Triple, Carnage, Brood,
                                 Loin, LongueDistance, Maitre };
    [[nodiscard]] KillAnnouncement killAnnouncement() const noexcept {
        return m_killAnnounceTimer > 0.0f ? m_killAnnounce : KillAnnouncement::None;
    }

    /* Prime de tir lointain, en trois paliers. La distance est celle du canon
       au point d'impact (RocketSystem::UpdateResult::explosionRangesM) : dans
       l'espace, pas au sol, un tir plongeant de 300 m valant un tir tendu de
       300 m. Le dernier palier demande de la hauteur : une roquette tirée à
       plat depuis 30 m touche le sol vers 290 m, il faut monter pour lui donner
       le temps d'aller plus loin. Prime attachée à l'EXPLOSION, pas au zombie :
       un double kill lointain ne la double pas, il cumule simplement avec la
       prime de kill multiple (killScoreForCount). */
    static constexpr float TIR_LOIN_M      = 150.0f;
    static constexpr float TIR_LONGUE_M    = 300.0f;
    static constexpr float TIR_MAITRE_M    = 400.0f;
    static constexpr int   TIR_LOIN_SCORE   = 50;
    static constexpr int   TIR_LONGUE_SCORE = 100;
    static constexpr int   TIR_MAITRE_SCORE = 200;

    [[nodiscard]] static int scoreDistance(float rangeM) noexcept {
        if (rangeM >= TIR_MAITRE_M) {
            return TIR_MAITRE_SCORE;
        }
        if (rangeM >= TIR_LONGUE_M) {
            return TIR_LONGUE_SCORE;
        }
        return rangeM >= TIR_LOIN_M ? TIR_LOIN_SCORE : 0;
    }

    /* Annonce propre au tir lointain, retenue seulement faute de kill multiple
       à annoncer : un double kill garde son libellé, la distance ne devenant
       alors qu'un suffixe (voir killAnnounceDistanceM). */
    [[nodiscard]] static KillAnnouncement annonceDistance(float rangeM) noexcept {
        if (rangeM >= TIR_MAITRE_M) {
            return KillAnnouncement::Maitre;
        }
        if (rangeM >= TIR_LONGUE_M) {
            return KillAnnouncement::LongueDistance;
        }
        return rangeM >= TIR_LOIN_M ? KillAnnouncement::Loin : KillAnnouncement::None;
    }

    /* Distance à afficher en suffixe de l'annonce courante, 0 si l'événement
       annoncé n'a pas atteint le premier palier (rien à ajouter). */
    [[nodiscard]] float killAnnounceDistanceM() const noexcept {
        return m_killAnnounceTimer > 0.0f ? m_killAnnounceDistanceM : 0.0f;
    }

    /* Flash de bouche courant (retour visuel du tir, indépendant du son -- voir
       ApplicationRenderEffects.cpp) : actif quelques dizaines de ms après
       chaque coup parti, pour rester visible même en rafale rapide. Position
       et direction du canon à cet instant, pour placer le flash devant
       l'appareil plutôt qu'en son centre. */
    [[nodiscard]] bool muzzleFlashActive() const noexcept { return m_muzzleFlashTimer > 0.0f; }

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
    /* Contenu d'une sphère, décidé par le nombre de zombies fauchés d'un coup :
       kérosène (bleue), vie (rouge) ou hécatombe (noire). */
    enum class BonusType { Carburant, Vie, Mort };

    /* Sphère prête pour le rendu : centre monde, facteur d'échelle de la
       sortie de terre (0 au ras du sol, 1 une fois monté) et opacité (pleine
       d'abord, puis décroissante sur les vingt dernières secondes). */
    struct BonusSphereView {
        vec3      center{0.0f};
        float     scale = 1.0f;
        float     alpha = 1.0f;
        BonusType type  = BonusType::Carburant;
        /* Vrai tant que la fusée monte ou retombe : le rendu dessine alors le
           tube noir, pas encore le sphere. La flamme, elle, ne sort qu'en montée
           (propulsion). */
        bool      enVol      = false;
        bool      propulsion = false;
    };

    /* Sphères larguées par un kill, un par sphère encore en place, dans
       l'ordre d'apparition (voir Application::renderCombatEntities). */
    [[nodiscard]] std::vector<BonusSphereView> bonusSpheres() const;

    /* Traces de brûlure au sol laissées par les impacts (décalques sombres qui
       s'estompent, voir Application::drawScorchMarks). */
    [[nodiscard]] std::vector<RocketSystem::ScorchView> scorches() const {
        return m_rockets.scorches();
    }

    /* Contact avec le sol, à la vitesse d'arrivée mesurée par la physique (voir
       physics::FlightModel::consumeGroundImpact). Au-delà de la vitesse tolérée,
       le choc fend le réservoir : il coûte du CARBURANT, proportionnellement à
       l'excès de vitesse, et fait le même bruit qu'un pneu reçu. La vie de
       l'appareil, elle, n'est entamée que par les zombies.

       Rend les litres à retirer, que l'appelant applique au modèle de vol
       (physics::FlightModel::drainFuel) : le combat décide du prix, la physique
       tient le réservoir. Rend 0 hors combat, partie perdue, ou sous le seuil --
       un posé normal ne coûte rien et ne s'entend pas. À appeler après update(),
       qui remet les événements sonores à zéro.

       'fuelLiters' est le contenu courant du réservoir : un choc ne le vide
       JAMAIS complètement, il s'arrête à la réserve (voir IMPACT_RESERVE_L).
       Sans ce plafond, la courbe au carré dépasse la contenance dès 20 m/s et
       tout contact un peu violent clouait l'appareil au sol, la partie perdue
       sans qu'aucun zombie y soit pour rien. Un réservoir déjà sous la réserve
       ne perd plus rien : il n'y a plus rien à prendre. */
    [[nodiscard]] float applyGroundImpact(float speedMs, float fuelLiters);
    /* Kérosène qu'un choc laisse toujours dans le réservoir. Calé sur le seuil
       du voyant bas carburant (physics::FUEL_LOW_L) : le pilote se relève avec
       l'alarme allumée et cinq à huit minutes de vol (112 à 194 L/h) pour
       trouver une sphère bleue. C'est une chance, pas un pardon. */
    static constexpr float IMPACT_RESERVE_L = physics::FUEL_LOW_L;

    /* Kérosène (L) ramassé pendant le dernier update() en traversant une sphère
       vert : à ajouter au modèle de vol (addFuel), à lire après update() comme
       shotFuelBurn. */
    [[nodiscard]] float bonusFuelPickup() const noexcept { return m_bonusFuelL; }

    /* Kérosène (L) brûlé par les coups partis pendant le dernier update() : le
       lance-roquettes puise dans le réservoir de l'appareil, si bien qu'arroser
       la horde en rafale coûte des minutes de vol (voir SHOT_FUEL_L, dans
       BonusSphereReglages.hpp). À appliquer au modèle de vol (drainFuel) comme le
       prix d'un choc au sol, et à lire après update(), qui repose les événements
       du pas. */
    [[nodiscard]] float shotFuelBurn() const noexcept {
        return m_events.fired ? SHOT_FUEL_L : 0.0f;
    }

private:
    static constexpr float PLAYER_HEALTH_MAX = 100.0f;
    /* Vitesse d'arrivée (m/s) en deçà de laquelle le contact est un posé et non
       un choc, puis litres perdus par (m/s) d'excès AU CARRÉ. Le carré plutôt
       qu'une droite : la fuite suit alors l'énergie du choc, si bien qu'une touche
       un peu ferme se paie en minutes de vol (8 L à 5 m/s, 50 L à 8 m/s) alors
       qu'un vrai crash vide le réservoir de 575 L et cloue l'appareil au sol
       (578 L à 20 m/s). Une droite faisait fuir trop de kérosène à chaque
       contact. */
    static constexpr float GROUND_IMPACT_FREE_MS    = 3.0f;
    static constexpr float GROUND_IMPACT_FUEL_COEFF = 2.0f;
    /* Perte en deçà de laquelle le contact ne compte pas : le HUD affiche le
       carburant en litres entiers (voir HudCorners.cpp), donc un demi-litre est la
       plus petite fuite qu'un joueur puisse VOIR. En dessous, on ne joue même pas
       le bruit du choc -- un son sans effet visible se lit comme un bug, et fait
       chercher la perte ailleurs. Avec le coefficient ci-dessus, cela place le
       premier vrai choc à 3,5 m/s d'arrivée. */
    static constexpr float GROUND_IMPACT_MIN_LITERS = 0.5f;

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
    float            m_killAnnounceDistanceM = 0.0f;
    float            m_killAnnounceTimer = 0.0f;  /* s restantes d'affichage de l'annonce */
    vec3             m_lastMuzzlePos{0.0f};
    vec3             m_lastFireDir{1.0f, 0.0f, 0.0f};
    /* Sphère posée par un kill : centre monde courant (elle monte pendant sa sortie
       de terre), altitude du sol sous lui d'où il est parti, échelle courante
       de cette sortie et temps qu'il lui reste à vivre. */
    struct BonusSphere {
        vec3  center{0.0f};
        float groundY    = 0.0f;
        float scale      = 0.0f;
        float remainingS = 0.0f;
        bool      enVol      = true;  /* fusée en route ; faux dès l'éclosion */
        bool      propulsion = true;  /* moteur allumé : montée seulement */
        BonusType type       = BonusType::Carburant;
    };
    std::vector<BonusSphere> m_bonusSpheres;
    /* Tirage au sort du lancement de fusée (chanceFuseeBonus). Propre au mode :
       le semer depuis les autres générateurs ferait dépendre la chance du
       nombre de zombies déjà apparus. */
    std::mt19937             m_bonusRng{std::random_device{}()};
    /* Hécatombe en cours (sphère noire ramassée) : temps restant avant la
       prochaine mise à mort. Négatif quand aucune n'est en cours. */
    float                    m_hecatombeTimer = -1.0f;
    float                  m_bonusFuelL = 0.0f;  /* litres ramassés au dernier update() */
    SoundEvents      m_events;                /* réinitialisés à chaque update() */
    ZombieHorde      m_horde;
    Weapon           m_weapon;
    RocketSystem     m_rockets;
    ProjectileSystem m_projectiles;
    WaveManager      m_waves;
};

}  /* namespace artouste::app */
