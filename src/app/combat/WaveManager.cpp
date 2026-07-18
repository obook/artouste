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

int zombiesForWave(int wave) noexcept {
    return std::min(BASE_ZOMBIES + (wave - 1) * ZOMBIES_STEP, MAX_ZOMBIES_PER_WAVE);
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
    beginWave(1, &horde);
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

void WaveManager::beginWave(int number, ZombieHorde* immediateSpawnHorde) noexcept {
    m_waveNumber   = number;
    m_waveElapsedS = 0.0f;

    const int count = zombiesForWave(number);
    if (immediateSpawnHorde != nullptr) {
        for (int i = 0; i < count; ++i) {
            spawnOne(*immediateSpawnHorde);
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
        const bool cleared  = horde.count() == 0;
        const bool timedOut = m_waveElapsedS > WAVE_MAX_DURATION_S;
        if (cleared || timedOut) {
            beginWave(m_waveNumber + 1);
        }
    }

    return difficultyForWave(m_waveNumber);
}

}  /* namespace artouste::app */
