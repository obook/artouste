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
#include <cstddef>

namespace artouste::app {

namespace {
/* Rayon (m) de la sphère de collision de l'appareil pour les pneus
   toxiques : englobant large plutôt qu'une forme précise, cohérent avec le
   reste du mode (mitrailleuse elle aussi en sphères simples).

   2,5 m ne couvrait que la bulle de la cabine, sur une Alouette II qui mesure
   près de dix mètres poutre de queue comprise : un pneu qui passait
   visiblement dans la machine la traversait sans rien déclencher, ni dégâts ni
   bruit d'impact. 4 m englobe la cabine et une bonne part de la poutre, au prix
   d'un englobant encore généreux sous le rotor -- moindre mal comparé à des
   impacts visibles restés sans effet. */
constexpr float HELI_HIT_RADIUS_M = 4.0f;
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

/* Contenu de la sphère lâchée par une explosion, selon le nombre de zombies
   qu'elle a fauchés : mêmes seuils que l'annonce HUD (double, triple). */
CombatMode::BonusType bonusTypePourKills(int killCount) noexcept {
    if (killCount >= BONUS_MORT_KILL_MIN) {
        return CombatMode::BonusType::Mort;
    }
    if (killCount >= BONUS_SANTE_KILL_MIN) {
        return CombatMode::BonusType::Vie;
    }
    return CombatMode::BonusType::Carburant;
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
    /* Repart d'un état propre : sans quoi les pneus toxiques et roquettes
       d'une partie précédente restent en vol au relancement ("pluie de
       pneus"). */
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
    m_broodWasAlive     = m_horde.broodAlive();
    m_bonusSpheres.clear();
    m_hecatombeTimer = -1.0f;
}

void CombatMode::stop() noexcept {
    m_active = false;
    /* Sans ce reset, mourir en arène (m_gameOver = true, voir update()) laissait
       le drapeau collé pour toute la suite de la session : sur la carte suivante
       (non zombie, où seul stop() est appelé, jamais start()), le bouton A du
       gestionnaire d'entrées restait détourné vers "confirmer le retour au menu"
       (voir ApplicationInput.cpp, branche m_combat.gameOver()) au lieu de son
       usage normal (livrée). */
    m_gameOver = false;
    m_horde.clear();
    m_projectiles.clear();
    m_rockets.clear();
    m_bonusSpheres.clear();
    m_hecatombeTimer = -1.0f;
}

void CombatMode::update(float dt, const physics::RigidBody& body, bool fireTrigger,
                        const std::function<float(float, float)>& terrainHeight) noexcept {
    m_events     = SoundEvents{};  /* aucun événement tant que rien ne s'est produit ce pas */
    m_bonusFuelL = 0.0f;           /* idem : le ramassage ne vaut que pour ce pas */
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

    /* Sphères du kill : vieillissent, puis disparaissent d'un coup. L'appareil
       qui en traverse un fait le plein d'autant, et la sphère s'efface aussitôt
       (même englobant sphérique que pour les pneus toxiques, élargi du
       demi-côté de la sphère et d'une marge invisible : le mode entier se contente de
       sphères, et rater le bidon de peu serait injuste). */
    const float pickupRadius =
        HELI_HIT_RADIUS_M + BONUS_SPHERE_HALF_M + BONUS_SPHERE_PICKUP_MARGIN_M;
    for (BonusSphere& sphere : m_bonusSpheres) {
        sphere.remainingS -= dt;
        /* Chandelle de feu d'artifice, en trois temps. La graine part du sol et
           monte en freinant (progression en 1-(1-t)^2 : la moitié de la hauteur
           est prise dans le premier tiers du temps, le reste s'étire) jusqu'à
           son sommet ; elle en redescend doucement, à peine d'abord puis un peu
           plus vite (progression en u^2), jusqu'à l'altitude de la sphère ; là, elle
           s'ouvre presque d'un coup. */
        const float age = std::max(0.0f, BONUS_SPHERE_LIFE_S - sphere.remainingS);
        if (age < BONUS_SPHERE_RISE_S) {
            const float t     = age / BONUS_SPHERE_RISE_S;
            const float reste = (1.0f - t) * (1.0f - t);
            sphere.center.y     = sphere.groundY + (1.0f - reste) * BONUS_SPHERE_APEX_M;
        } else {
            const float u = std::min(1.0f, (age - BONUS_SPHERE_RISE_S) / BONUS_SPHERE_FALL_S);
            sphere.center.y =
                sphere.groundY + BONUS_SPHERE_APEX_M - u * u * (BONUS_SPHERE_APEX_M - BONUS_SPHERE_AGL_M);
        }

        /* Instant précis où l'éclosion commence : la graine vient de finir sa
           retombée à ce pas (comparaison d'état comme ailleurs dans le mode). */
        const float openAge = BONUS_SPHERE_RISE_S + BONUS_SPHERE_FALL_S;
        if (age >= openAge && age - dt < openAge) {
            switch (sphere.type) {
                case BonusType::Vie:
                    m_events.bonusOpenSantePositions.push_back(sphere.center);
                    break;
                case BonusType::Mort:
                    m_events.bonusOpenMortPositions.push_back(sphere.center);
                    break;
                case BonusType::Carburant:
                    m_events.bonusOpenPositions.push_back(sphere.center);
                    break;
            }
        }
        sphere.enVol      = age < openAge;
        sphere.propulsion = age < BONUS_SPHERE_RISE_S;

        const float grow = std::min(1.0f, std::max(0.0f, age - openAge) / BONUS_SPHERE_GROW_S);
        const float open = 1.0f - (1.0f - grow) * (1.0f - grow);
        sphere.scale       = BONUS_SPHERE_SEED_SCALE + open * (1.0f - BONUS_SPHERE_SEED_SCALE);
        if (glm::distance(body.position, sphere.center) <= pickupRadius) {
            switch (sphere.type) {
                case BonusType::Vie:
                    m_playerHealth =
                        std::min(PLAYER_HEALTH_MAX,
                                 m_playerHealth + BONUS_SANTE_FRACTION * PLAYER_HEALTH_MAX);
                    break;
                case BonusType::Mort:
                    /* Hécatombe amorcée : la première victime tombe au prochain
                       pas, les autres suivent une par une (voir plus bas). */
                    m_hecatombeTimer = 0.0f;
                    break;
                case BonusType::Carburant:
                    m_bonusFuelL += BONUS_SPHERE_FUEL_L;
                    break;
            }
            m_events.bonusPickup = true;
            sphere.remainingS = 0.0f;
        }
    }
    m_bonusSpheres.erase(std::remove_if(m_bonusSpheres.begin(), m_bonusSpheres.end(),
                                      [](const BonusSphere& c) { return c.remainingS <= 0.0f; }),
                       m_bonusSpheres.end());

    /* Hécatombe de la sphère noire : une mise à mort par intervalle, du plus
       proche de l'appareil au plus lointain, chacune avec sa boule de feu et son
       cri (mêmes effets que les marcheurs qui partent avec leur largueur). Le
       rythme rend la vague de morts lisible, là où tout tuer d'un coup ne
       faisait qu'un fracas. Le largueur y échappe (voir killNearest) : il reste
       à abattre à la roquette. */
    if (m_hecatombeTimer >= 0.0f) {
        m_hecatombeTimer -= dt;
        while (m_hecatombeTimer <= 0.0f) {
            vec3 mort{0.0f};
            if (!m_horde.killNearest(body.position, mort)) {
                m_hecatombeTimer = -1.0f;  /* plus personne debout */
                break;
            }
            m_rockets.addExplosion(mort);
            m_events.explosionPositions.push_back(mort);
            m_events.zombieDeathPositions.push_back(mort);
            m_kills += 1;
            m_score += BONUS_MORT_SCORE;
            m_hecatombeTimer += BONUS_MORT_INTERVALLE_S;
        }
    }
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
    for (std::size_t i = 0; i < rocketRes.explosionKillCounts.size(); ++i) {
        const int killCount = rocketRes.explosionKillCounts[i];
        m_score += killScoreForCount(killCount);
        const KillAnnouncement ann = killAnnouncementForCount(killCount);
        if (ann != KillAnnouncement::None) {
            m_killAnnounce      = ann;
            m_killAnnounceTimer = KILL_ANNOUNCE_DURATION_S;
        }
        /* Une fusée part du point d'explosion dès qu'elle a fauché assez de
           zombies (BONUS_SPHERE_KILL_MIN) poser un volume : un bidon de kérosène,
           ou une trousse de secours à partir du double kill. Hauteur prise sur
           le RELIEF sous ce point, pas sur l'altitude de l'explosion elle-même,
           qui peut avoir eu lieu en plein vol au contact d'un zombie. */
        if (killCount >= BONUS_SPHERE_KILL_MIN && i < rocketRes.explosionPositions.size()) {
            const vec3  pos     = rocketRes.explosionPositions[i];
            const float groundY = terrainHeight(pos.x, pos.z);
            /* Posé au ras du sol et de taille nulle : la montée de l'image
               suivante lui donne son altitude et sa taille. */
            m_bonusSpheres.push_back(
                BonusSphere{vec3{pos.x, groundY, pos.z}, groundY, 0.0f, BONUS_SPHERE_LIFE_S,
                            true, true, bonusTypePourKills(killCount)});
            m_events.bonusLaunchPositions.push_back(vec3{pos.x, groundY, pos.z});
        }
    }

    /* Largueur neutralisé : prime de score et annonce dédiée, qui écrase un
       éventuel kill multiple de la même explosion (l'événement marquant, c'est
       le boss). Détecté après la mise à jour des roquettes, seule source de
       dégâts capable de l'entamer. */
    const bool broodAlive = m_horde.broodAlive();
    if (m_broodWasAlive && !broodAlive) {
        /* Ce que le largueur a lâché ne lui survit pas : ses marcheurs éclatent
           sur place. Une boule de feu et un cri par marcheur, et ils comptent
           comme des mises à mort -- le joueur les a bien gagnées en abattant le
           boss. L'annonce, elle, reste celle du largueur : elle est posée après,
           pour qu'un éventuel carnage simultané ne la vole pas. */
        const std::vector<vec3> eclates = m_horde.killBroodlings();
        for (const vec3& pos : eclates) {
            m_rockets.addExplosion(pos);
            m_events.explosionPositions.push_back(pos);
            m_events.zombieDeathPositions.push_back(pos);
        }
        m_kills += static_cast<int>(eclates.size());
        m_score += BROODLING_SCORE * static_cast<int>(eclates.size());

        m_score += BROOD_SCORE;
        m_killAnnounce      = KillAnnouncement::Brood;
        m_killAnnounceTimer = KILL_ANNOUNCE_DURATION_S;
    }
    /* Apparition : le gestionnaire de vagues vient de la poser (manche de boss),
       c'est le moment du râle. */
    m_events.broodSpawned = !m_broodWasAlive && broodAlive;
    m_broodWasAlive       = broodAlive;

    const float damage = m_projectiles.update(dt, body.position, HELI_HIT_RADIUS_M);
    if (damage > 0.0f) {
        m_events.impacted = true;
        m_playerHealth     = std::max(0.0f, m_playerHealth - damage);
        if (m_playerHealth <= 0.0f) {
            m_gameOver = true;
        }
    }
}

float CombatMode::applyGroundImpact(float speedMs) {
    if (!m_active || m_gameOver || speedMs <= GROUND_IMPACT_FREE_MS) {
        return 0.0f;
    }
    /* Seul l'excès de vitesse compte, et il compte au carré : les premiers mètres
       par seconde au-delà du posé passent dans les patins, les suivants ouvrent
       le réservoir. */
    const float exces  = speedMs - GROUND_IMPACT_FREE_MS;
    const float litres = exces * exces * GROUND_IMPACT_FUEL_COEFF;
    /* Sous un demi-litre, la jauge affiche encore le même nombre entier : le
       joueur entendrait le choc sans rien voir bouger. Un contact aussi doux est
       un posé, pas un choc : ni fuite ni bruit. */
    if (litres < GROUND_IMPACT_MIN_LITERS) {
        return 0.0f;
    }
    m_events.impacted = true;
    return litres;
}

std::vector<CombatMode::BonusSphereView> CombatMode::bonusSpheres() const {
    std::vector<BonusSphereView> vues;
    vues.reserve(m_bonusSpheres.size());
    for (const BonusSphere& sphere : m_bonusSpheres) {
        const float alpha = std::min(1.0f, sphere.remainingS / BONUS_SPHERE_FADE_S);
        vues.push_back(
            BonusSphereView{sphere.center, sphere.scale, alpha, sphere.type, sphere.enVol,
                            sphere.propulsion});
    }
    return vues;
}

}  /* namespace artouste::app */
