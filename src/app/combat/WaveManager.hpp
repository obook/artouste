/*
 * WaveManager.hpp
 * Gestionnaire de vagues du mode zombie : lit les points de spawn de la carte
 * (zombies.txt) une fois au démarrage, peuple la première vague d'un coup,
 * puis pilote les vagues suivantes (spawn échelonné sur quelques secondes,
 * nombre de zombies et difficulté croissants) une fois la vague courante
 * exterminée ou son délai maximal dépassé (anti-blocage).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/combat/ZombieHorde.hpp"
#include "util/Math.hpp"

#include <filesystem>
#include <random>
#include <vector>

namespace artouste::app {

class WaveManager {
public:
    /* Charge <terrainDir>/zombies.txt (points de spawn, "x z" par ligne) et
       peuple immédiatement la première vague dans horde (pas de spawn
       échelonné pour la toute première vague : l'effet "ils arrivent" se
       joue dès l'entrée en jeu). Renvoie faux si le fichier est absent ou
       vide (carte non compatible) : horde reste alors inchangée. */
    bool start(const std::filesystem::path& terrainDir, ZombieHorde& horde) noexcept;

    /* Avance le gestionnaire d'un pas de temps : détecte la fin de la vague
       courante (horde exterminée ou délai maximal dépassé), enchaîne alors
       sur la suivante avec un spawn échelonné sur quelques secondes ; pendant
       le spawn échelonné, fait apparaître les zombies restants au rythme de
       SPAWN_INTERVAL_S. Renvoie le facteur de difficulté courant (vitesse de
       marche, fréquence de jet), à transmettre à ZombieHorde::update. */
    float update(float dt, ZombieHorde& horde) noexcept;

    /* Une manche sur cinq est une manche de boss : un largueur
       (ZombieHorde::Type::Brood) y apparaît d'emblée, escorté de la moitié
       seulement des marcheurs habituels, et lâche ensuite un marcheur toutes
       les BROOD_SPAWN_INTERVAL_S secondes tant qu'il est debout. La manche ne
       peut donc se terminer qu'en l'abattant. */
    static constexpr int BOSS_WAVE_INTERVAL = 5;
    [[nodiscard]] static bool isBossWave(int wave) noexcept {
        return wave > 0 && wave % BOSS_WAVE_INTERVAL == 0;
    }

    [[nodiscard]] int   waveNumber() const noexcept { return m_waveNumber; }
    [[nodiscard]] float waveElapsedS() const noexcept { return m_waveElapsedS; }
    /* Vagues intégralement survécues (la vague en cours ne compte pas tant
       qu'elle n'est pas exterminée). */
    [[nodiscard]] int score() const noexcept { return m_waveNumber > 0 ? m_waveNumber - 1 : 0; }

private:
    enum class Phase { Spawning, Fighting };

    void spawnOne(ZombieHorde& horde) noexcept;
    /* Ouvre une manche. Le largueur d'une manche de boss apparaît toujours
       d'emblée (le joueur doit le voir arriver) ; immediateEscort ne concerne
       que les marcheurs qui l'accompagnent, peuplés d'un coup pour la manche 1
       (voir start) et échelonnés ensuite. */
    void beginWave(int number, ZombieHorde& horde, bool immediateEscort) noexcept;
    /* Fait apparaître un marcheur autour du largueur (à quelques mètres, dans
       une direction quelconque) : la relève arrive du boss lui-même et non des
       points de spawn du bord de carte, sans quoi rien ne rattacherait
       visuellement ces zombies à lui. */
    void spawnFromBrood(ZombieHorde& horde) noexcept;

    std::vector<vec3> m_spawnPoints;
    Phase              m_phase          = Phase::Fighting;
    int                m_waveNumber     = 0;
    float              m_waveElapsedS   = 0.0f;
    float              m_spawnTimerS    = 0.0f;
    int                m_zombiesToSpawn = 0;
    float              m_broodTimerS    = 0.0f;  /* avant le prochain largage du largueur */
    std::mt19937       m_rng{std::random_device{}()};
};

}  /* namespace artouste::app */
