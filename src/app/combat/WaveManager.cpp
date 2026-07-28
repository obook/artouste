/*
 * WaveManager.cpp
 * Voir WaveManager.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/WaveManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>

namespace artouste::app {

namespace {
constexpr int   BASE_ZOMBIES         = 5;    /* vague 1 */
constexpr int   ZOMBIES_STEP         = 3;    /* zombies ajoutés par vague */
constexpr int   MAX_ZOMBIES_PER_WAVE = 60;   /* plafond (marge large sous la capacité GPU, 300) */
constexpr float SPAWN_INTERVAL_S     = 0.6f; /* entre deux apparitions (vagues 2+) */
constexpr float WAVE_MAX_DURATION_S  = 90.0f; /* anti-blocage (zombie coincé, relief) */
constexpr float SPEED_STEP           = 0.12f; /* += facteur de difficulté par vague */
constexpr float MAX_DIFFICULTY       = 2.5f;   /* plafond du facteur de difficulté */

/* Manche de boss : la pondeuse remplace la moitié de l'escorte habituelle (le
   reste de la pression vient de ce qu'elle engendre), et pond un marcheur à cet
   intervalle tant qu'elle vit. Ces apparitions s'arrêtent au plafond de vague
   pour ne pas laisser la horde enfler sans fin si le joueur tarde à l'abattre. */
constexpr float BROOD_SPAWN_INTERVAL_S = 3.0f;
/* Distances (m) autour de la pondeuse où atterrit sa portée. */
constexpr float BROOD_SPAWN_RADIUS_MIN_M = 3.0f;
constexpr float BROOD_SPAWN_RADIUS_MAX_M = 9.0f;

int zombiesForWave(int wave) noexcept {
    const int count = std::min(BASE_ZOMBIES + (wave - 1) * ZOMBIES_STEP, MAX_ZOMBIES_PER_WAVE);
    return WaveManager::isBossWave(wave) ? std::max(1, count / 2) : count;
}

float difficultyForWave(int wave) noexcept {
    return std::min(1.0f + static_cast<float>(wave - 1) * SPEED_STEP, MAX_DIFFICULTY);
}
}  /* namespace */

bool WaveManager::start(const std::filesystem::path& terrainDir, ZombieHorde& horde) noexcept {
    m_spawnPoints.clear();

    const std::filesystem::path spawnFile = terrainDir / "zombies.txt";
    std::ifstream                in(spawnFile);
    if (!in) {
        return false;  /* fichier absent : carte non compatible, pas une erreur */
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream iss(line);
        float              x = 0.0f, z = 0.0f;
        if (iss >> x >> z) {
            m_spawnPoints.emplace_back(x, 0.0f, z);
        }
    }
    if (m_spawnPoints.empty()) {
        std::fprintf(stderr, "[WaveManager] %s vide : mode zombie indisponible sur cette carte.\n",
                     spawnFile.string().c_str());
        return false;
    }

    /* Première vague peuplée d'un coup (pas de spawn échelonné) : l'effet
       "ils arrivent" se joue dès l'entrée en jeu, sans attendre la première
       image de mise à jour. */
    beginWave(1, horde, true);
    std::printf("[WaveManager] %zu points de spawn depuis %s, vague 1 : %zu zombies.\n",
               m_spawnPoints.size(), spawnFile.string().c_str(), horde.count());
    return true;
}

void WaveManager::spawnOne(ZombieHorde& horde) noexcept {
    if (m_spawnPoints.empty()) {
        return;
    }
    std::uniform_int_distribution<std::size_t> pointDist(0, m_spawnPoints.size() - 1);
    std::uniform_real_distribution<float>      yawDist(0.0f, TWO_PI);
    std::uniform_real_distribution<float>      phaseDist(0.0f, TWO_PI);
    horde.spawn(m_spawnPoints[pointDist(m_rng)], yawDist(m_rng), phaseDist(m_rng));
}

void WaveManager::spawnFromBrood(ZombieHorde& horde) noexcept {
    std::uniform_real_distribution<float> angleDist(0.0f, TWO_PI);
    std::uniform_real_distribution<float> radiusDist(BROOD_SPAWN_RADIUS_MIN_M,
                                                     BROOD_SPAWN_RADIUS_MAX_M);
    std::uniform_real_distribution<float> phaseDist(0.0f, TWO_PI);

    const float angle  = angleDist(m_rng);
    const float radius = radiusDist(m_rng);
    const vec3  around = horde.broodPosition() +
                        vec3{std::sin(angle) * radius, 0.0f, std::cos(angle) * radius};
    /* L'altitude est recalée sur le relief au prochain ZombieHorde::update,
       comme pour les apparitions ordinaires (voir spawnOne). */
    horde.spawnBroodling(around, angleDist(m_rng), phaseDist(m_rng));
}

void WaveManager::beginWave(int number, ZombieHorde& horde, bool immediateEscort) noexcept {
    m_waveNumber   = number;
    m_waveElapsedS = 0.0f;
    m_broodTimerS  = BROOD_SPAWN_INTERVAL_S;

    if (isBossWave(number) && !m_spawnPoints.empty()) {
        std::uniform_int_distribution<std::size_t> pointDist(0, m_spawnPoints.size() - 1);
        std::uniform_real_distribution<float>      yawDist(0.0f, TWO_PI);
        horde.spawnBrood(m_spawnPoints[pointDist(m_rng)], yawDist(m_rng), yawDist(m_rng));
    }

    const int count = zombiesForWave(number);
    if (immediateEscort) {
        for (int i = 0; i < count; ++i) {
            spawnOne(horde);
        }
        m_zombiesToSpawn = 0;
        m_spawnTimerS    = 0.0f;
        m_phase          = Phase::Fighting;
    } else {
        m_zombiesToSpawn = count;
        m_spawnTimerS    = 0.0f;  /* premier zombie dès le prochain update() */
        m_phase          = Phase::Spawning;
    }
}

float WaveManager::update(float dt, ZombieHorde& horde) noexcept {
    m_waveElapsedS += dt;

    /* Portée de la pondeuse : elle engendre pendant toute la manche de boss,
       phase d'apparition de l'escorte comprise, et s'arrête au plafond de
       vague pour ne pas laisser la horde enfler sans fin. */
    if (horde.broodAlive()) {
        m_broodTimerS -= dt;
        while (m_broodTimerS <= 0.0f) {
            if (horde.count() < static_cast<std::size_t>(MAX_ZOMBIES_PER_WAVE)) {
                spawnFromBrood(horde);
            }
            m_broodTimerS += BROOD_SPAWN_INTERVAL_S;
        }
    }

    if (m_phase == Phase::Spawning) {
        m_spawnTimerS -= dt;
        while (m_zombiesToSpawn > 0 && m_spawnTimerS <= 0.0f) {
            spawnOne(horde);
            --m_zombiesToSpawn;
            m_spawnTimerS += SPAWN_INTERVAL_S;
        }
        if (m_zombiesToSpawn == 0) {
            m_phase = Phase::Fighting;
        }
    } else {
        const bool cleared = horde.count() == 0;
        /* L'anti-blocage ne s'applique pas tant que la pondeuse tient debout :
           une manche de boss se gagne en l'abattant, pas en patientant. */
        const bool timedOut = !horde.broodAlive() && m_waveElapsedS > WAVE_MAX_DURATION_S;
        if (cleared || timedOut) {
            beginWave(m_waveNumber + 1, horde, false);
        }
    }

    return difficultyForWave(m_waveNumber);
}

}  /* namespace artouste::app */
