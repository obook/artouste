/*
 * CombatMode.cpp
 * Voir CombatMode.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/CombatMode.hpp"

#include <algorithm>

namespace artouste::app {

namespace {
/* Rayon (m) de la sphère de collision de l'appareil pour les boulettes
   toxiques : englobant large plutôt qu'une forme précise, cohérent avec le
   reste du mode (mitrailleuse elle aussi en sphères simples). */
constexpr float HELI_HIT_RADIUS_M = 2.5f;
/* Durée d'affichage du flash de bouche après un coup parti (s) : assez long
   pour rester visible à l'oeil même à pleine cadence (12 coups/s, un flash
   toutes les ~83 ms), sans jamais tout à fait s'éteindre entre deux coups. */
constexpr float MUZZLE_FLASH_DURATION_S = 0.10f;
/* Durée d'affichage de l'annonce de kill multiple (double/triple/carnage), en
   secondes : assez long pour être lu, assez court pour ne pas s'attarder si un
   autre kill multiple survient juste après. */
constexpr float KILL_ANNOUNCE_DURATION_S = 1.8f;

/* Points accordés pour les zombies tués par UNE MÊME explosion : bonifie les
   kills multiples plutôt que de compter 25 points par zombie indépendamment,
   pour récompenser un tir qui fauche plusieurs zombies groupés. Au-delà de 3,
   le bonus n'augmente plus (125 reste le plafond, triple kill ou plus). */
int killScoreForCount(int killCount) noexcept {
    switch (killCount) {
        case 0:  return 0;
        case 1:  return 25;
        case 2:  return 75;
        default: return 125;  /* triple kill et plus */
    }
}

/* Annonce HUD correspondant a un nombre de zombies tués par la même explosion,
   mêmes seuils que killScoreForCount : None sous 2 (rien à annoncer pour un
   kill simple, trop fréquent pour être une "annonce"). */
CombatMode::KillAnnouncement killAnnouncementForCount(int killCount) noexcept {
    if (killCount >= 4) {
        return CombatMode::KillAnnouncement::Carnage;
    }
    if (killCount == 3) {
        return CombatMode::KillAnnouncement::Triple;
    }
    if (killCount == 2) {
        return CombatMode::KillAnnouncement::Double;
    }
    return CombatMode::KillAnnouncement::None;
}
}  /* namespace */

void CombatMode::start(const std::filesystem::path& terrainDir,
                       const std::function<float(float, float)>& terrainHeight) noexcept {
    m_horde.clear();
    /* Repart d'un état propre : sans quoi les boulettes toxiques et roquettes
       d'une partie précédente restent en vol au relancement ("pluie de
       boulettes"). */
    m_projectiles.clear();
    m_rockets.clear();
    m_active = m_waves.start(terrainDir, m_horde);
    if (m_active) {
        /* Cale d'emblée l'altitude sur le relief (sans avancer l'IA : aucune
           position de joueur pertinente avant la première image de jeu). */
        m_horde.snapToGround(terrainHeight);
        /* La manche 1 est peuplée ici, avant le premier update() : son
           changement de numéro ne sera donc jamais observé par la comparaison
           d'état habituelle (voir plus bas). On force son annonce sonore au
           prochain update(), pour qu'elle sonne comme les suivantes. */
        m_firstWavePending = true;
    }

    m_playerHealth      = PLAYER_HEALTH_MAX;
    m_gameOver          = false;
    m_elapsedS          = 0.0f;
    m_kills             = 0;
    m_score             = 0;
    m_killAnnounce      = KillAnnouncement::None;
    m_killAnnounceTimer = 0.0f;
}

void CombatMode::stop() noexcept {
    m_active = false;
    m_horde.clear();
    m_projectiles.clear();
    m_rockets.clear();
}

void CombatMode::update(float dt, const physics::RigidBody& body, bool fireTrigger,
                        const std::function<float(float, float)>& terrainHeight) noexcept {
    m_events = SoundEvents{};  /* aucun événement tant que rien ne s'est produit ce pas */
    if (!m_active || m_gameOver) {
        return;
    }
    if (m_firstWavePending) {
        m_events.waveStart = true;
        m_firstWavePending = false;
    }
    m_elapsedS += dt;

    /* Canon fixe sous l'appareil : tire droit devant (repère corps, axe X),
       comme un affût de sabord plutôt qu'un viseur libre. */
    const vec3 muzzlePos = body.position;
    const vec3 fireDir   = glm::normalize(body.orientation * vec3{1.0f, 0.0f, 0.0f});
    const Weapon::FireResult fireResult = m_weapon.update(dt, fireTrigger);
    m_events.fired = fireResult.fired;

    /* Flash de bouche : retour visuel du départ de roquette, indépendant du son
       (voir ApplicationRenderEffects.cpp). Réarmé à chaque tir, décompte
       sinon. Une roquette part du canon visible (même point que le flash) dans
       l'axe de tir. */
    m_muzzleFlashTimer  = std::max(0.0f, m_muzzleFlashTimer - dt);
    m_killAnnounceTimer = std::max(0.0f, m_killAnnounceTimer - dt);
    if (fireResult.fired) {
        m_muzzleFlashTimer = MUZZLE_FLASH_DURATION_S;
        m_lastMuzzlePos    = muzzlePos;
        m_lastFireDir      = fireDir;
        m_events.muzzlePos = muzzleVisualPos();
        m_rockets.spawn(muzzleVisualPos(), fireDir);
    }

    /* Vagues : décide des spawns (nombre, étalement, difficulté) avant que la
       horde n'avance, pour que les zombies tout juste apparus marchent et
       recalent leur altitude dès cette même image. Une nouvelle vague vient
       de commencer si son numéro a changé (le ou= préserve l'annonce forcée
       de la manche 1 ci-dessus, qu'aucun changement de numéro ne détecterait
       ici puisqu'elle est déjà en place avant ce premier update()). */
    const int   waveBefore  = m_waves.waveNumber();
    const float difficulty  = m_waves.update(dt, m_horde);
    m_events.waveStart      = m_events.waveStart || (m_waves.waveNumber() != waveBefore);

    /* Hauteur du joueur au-dessus du sol : détermine si les zombies peuvent
       viser (voir ZombieHorde::TOXIC_CEILING_M). Mémorisée pour belowCeiling
       (HUD), lue en dehors de cette mise à jour. */
    m_lastPlayerAgl = body.position.y - terrainHeight(body.position.x, body.position.z);
    const std::vector<ThrowRequest> throwRequests =
        m_horde.update(dt, body.position, m_lastPlayerAgl, difficulty, terrainHeight);
    for (const ThrowRequest& req : throwRequests) {
        m_events.throwPositions.push_back(req.origin);
        m_projectiles.spawn(req.origin, req.target);
    }

    /* Roquettes du joueur : avancent, explosent au sol (ou au contact d'un
       zombie) et tuent en zone. Fait après le déplacement de la horde pour
       viser les positions de zombies de cette image. Positions à grain fin :
       une entrée par zombie touché/tué (pas par explosion), pour qu'un souffle
       fauchant plusieurs zombies d'un coup fasse entendre autant de cris
       distincts plutôt qu'un seul. */
    const RocketSystem::UpdateResult rocketRes = m_rockets.update(dt, terrainHeight, m_horde);
    m_events.explosionPositions   = rocketRes.explosionPositions;
    m_events.zombieHitPositions   = rocketRes.zombieHitPositions;
    m_events.zombieDeathPositions = rocketRes.zombieDeathPositions;
    m_kills += rocketRes.kills;
    for (int killCount : rocketRes.explosionKillCounts) {
        m_score += killScoreForCount(killCount);
        const KillAnnouncement ann = killAnnouncementForCount(killCount);
        if (ann != KillAnnouncement::None) {
            m_killAnnounce      = ann;
            m_killAnnounceTimer = KILL_ANNOUNCE_DURATION_S;
        }
    }

    const float damage = m_projectiles.update(dt, body.position, HELI_HIT_RADIUS_M);
    if (damage > 0.0f) {
        m_events.impacted = true;
        m_playerHealth     = std::max(0.0f, m_playerHealth - damage);
        if (m_playerHealth <= 0.0f) {
            m_gameOver = true;
        }
    }
}

}  /* namespace artouste::app */
