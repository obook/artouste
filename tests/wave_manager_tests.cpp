/*
 * wave_manager_tests.cpp
 * Tests de l'orchestration des vagues du mode zombie (WaveManager) : première
 * vague, escalade de difficulté, score et manches de boss (pondeuse). Se teste
 * sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/WaveManager.hpp"
#include "app/combat/ZombieHorde.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>

using artouste::app::WaveManager;
using artouste::app::ZombieHorde;

namespace {
/* Écrit un zombies.txt temporaire dans un dossier dédié (WaveManager attend
   un dossier de carte, comme CombatMode) et renvoie ce dossier. */
std::filesystem::path writeTempSpawnDir(const std::string& dirName, const std::string& content) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / dirName;
    std::filesystem::create_directories(dir);
    std::ofstream out(dir / "zombies.txt");
    out << content;
    return dir;
}
} /* namespace */

TEST_CASE("WaveManager : première vague, escalade et score", "[combat][waves]") {
    const auto dir = writeTempSpawnDir("artouste_waves_test", "10 0\n20 0\n30 0\n");

    SECTION("fichier absent : start() renvoie faux, horde inchangée") {
        WaveManager waves;
        ZombieHorde horde;
        CHECK_FALSE(waves.start("/chemin/qui/n/existe/pas", horde));
        CHECK(horde.count() == 0);
    }

    SECTION("première vague peuplée d'un coup, pas de spawn échelonné") {
        WaveManager waves;
        ZombieHorde horde;
        REQUIRE(waves.start(dir, horde));
        CHECK(waves.waveNumber() == 1);
        CHECK(horde.count() == 5); /* BASE_ZOMBIES */
        CHECK(waves.score() == 0); /* vague 1 pas encore survécue */
    }

    SECTION("vague suivante déclenchée par l'extermination, difficulté accrue") {
        WaveManager waves;
        ZombieHorde horde;
        REQUIRE(waves.start(dir, horde));

        horde.clear(); /* simule l'extermination de la vague 1 */
        const float difficulty = waves.update(0.1f, horde);
        CHECK(waves.waveNumber() == 2);
        CHECK(waves.score() == 1); /* vague 1 survécue */
        CHECK(difficulty > 1.0f);  /* difficulté croissante dès la vague 2 */
        CHECK(horde.count() == 0); /* transition détectée, spawn pas encore joué */

        /* Draine le spawn échelonné de la vague 2 (BASE + ZOMBIES_STEP = 8). */
        for (int i = 0; i < 20; ++i) {
            waves.update(0.6f, horde);
        }
        CHECK(horde.count() == 8);
    }

    SECTION("délai maximal dépassé : vague suivante même sans horde vide") {
        WaveManager waves;
        ZombieHorde horde;
        REQUIRE(waves.start(dir, horde));
        waves.update(95.0f, horde); /* > WAVE_MAX_DURATION_S (90 s), anti-blocage */
        CHECK(waves.waveNumber() == 2);
    }

    std::filesystem::remove_all(dir);
}

namespace {
/* Enchaîne les manches jusqu'à celle demandée en vidant la horde à chaque fois
   (extermination immédiate), puis draine le spawn échelonné de la manche
   atteinte. */
void avanceJusqua(WaveManager& waves, ZombieHorde& horde, int wave) {
    while (waves.waveNumber() < wave) {
        horde.clear();
        waves.update(0.1f, horde);
    }
    for (int i = 0; i < 60; ++i) {
        waves.update(0.6f, horde);
    }
}
} /* namespace */

TEST_CASE("WaveManager : manches de boss (pondeuse)", "[combat][waves][boss]") {
    const auto dir = writeTempSpawnDir("artouste_boss_test", "10 0\n20 0\n30 0\n");

    SECTION("une manche sur cinq est une manche de boss") {
        CHECK_FALSE(WaveManager::isBossWave(0)); /* avant la première manche */
        CHECK_FALSE(WaveManager::isBossWave(1));
        CHECK_FALSE(WaveManager::isBossWave(4));
        CHECK(WaveManager::isBossWave(5));
        CHECK(WaveManager::isBossWave(10));
    }

    SECTION("la pondeuse apparaît dès l'ouverture de la manche 5") {
        WaveManager waves;
        ZombieHorde horde;
        REQUIRE(waves.start(dir, horde));
        CHECK_FALSE(horde.broodAlive()); /* manche 1 : pas de boss */

        /* Extermination des manches 1 à 4 : la pondeuse doit être debout dès
           l'instant où la manche 5 s'ouvre, avant même son escorte. */
        while (waves.waveNumber() < 5) {
            horde.clear();
            waves.update(0.1f, horde);
        }
        CHECK(waves.waveNumber() == 5);
        CHECK(horde.broodAlive());
        CHECK(horde.broodHealthPct() == Catch::Approx(1.0f));
    }

    SECTION("la pondeuse engendre des marcheurs tant qu'elle est debout") {
        WaveManager waves;
        ZombieHorde horde;
        REQUIRE(waves.start(dir, horde));
        avanceJusqua(waves, horde, 5);
        REQUIRE(horde.broodAlive());

        const std::size_t avant = horde.count();
        waves.update(3.5f, horde); /* > BROOD_SPAWN_INTERVAL_S (3 s) */
        CHECK(horde.count() > avant);
    }

    SECTION("l'anti-blocage ne clôt pas une manche dont la pondeuse tient debout") {
        WaveManager waves;
        ZombieHorde horde;
        REQUIRE(waves.start(dir, horde));
        avanceJusqua(waves, horde, 5);
        REQUIRE(horde.broodAlive());

        waves.update(95.0f, horde); /* > WAVE_MAX_DURATION_S : sans effet ici */
        CHECK(waves.waveNumber() == 5);
        CHECK(horde.broodAlive());
    }

    std::filesystem::remove_all(dir);
}
