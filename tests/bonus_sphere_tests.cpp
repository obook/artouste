/*
 * bonus_sphere_tests.cpp
 * Tests du tirage au sort de la fusée de bonus du mode zombie
 * (chanceFuseeBonus) : décroissance manche après manche, plancher, et prime
 * aux kills multiples. Se teste sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : septembre 2026
 * Licence : GPL v2
 */

#include "app/combat/BonusSphereReglages.hpp"
#include "app/combat/CombatMode.hpp"
#include "physics/RigidBody.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace physics = artouste::physics;

using artouste::vec3;
using artouste::app::BONUS_CHANCE_MANCHE1;
using artouste::app::BONUS_CHANCE_PLANCHER;
using artouste::app::chanceFuseeBonus;
using artouste::app::CombatMode;
using artouste::app::ZombieHorde;

namespace {

/* Sol plat au niveau de la mer, comme dans les autres tests de combat. */
float solPlat(float, float) {
    return 0.0f;
}

} /* namespace */

TEST_CASE("Fusée de bonus : chance décroissante et prime aux kills multiples",
          "[combat][bonus]") {
    SECTION("manche 1 : presque toujours, mais plus jamais garantie") {
        CHECK(chanceFuseeBonus(1, 1) == Catch::Approx(BONUS_CHANCE_MANCHE1));
        CHECK(chanceFuseeBonus(1, 1) < 1.0f);
    }

    SECTION("la chance décroît manche après manche") {
        float precedente = chanceFuseeBonus(1, 1);
        for (int wave = 2; wave <= 30; ++wave) {
            const float chance = chanceFuseeBonus(wave, 1);
            CHECK(chance < precedente);
            precedente = chance;
        }
    }

    SECTION("courbe visée : environ 70 % en manche 3, 35 % en manche 10") {
        CHECK(chanceFuseeBonus(3, 1) == Catch::Approx(0.70f).margin(0.03f));
        CHECK(chanceFuseeBonus(5, 1) == Catch::Approx(0.55f).margin(0.03f));
        CHECK(chanceFuseeBonus(10, 1) == Catch::Approx(0.35f).margin(0.03f));
    }

    SECTION("jamais sous le plancher, même très loin") {
        CHECK(chanceFuseeBonus(200, 1) >= BONUS_CHANCE_PLANCHER);
        CHECK(chanceFuseeBonus(200, 1) == Catch::Approx(BONUS_CHANCE_PLANCHER).margin(0.01f));
    }

    SECTION("un kill multiple améliore le tirage sans jamais le garantir") {
        CHECK(chanceFuseeBonus(10, 2) > chanceFuseeBonus(10, 1));
        CHECK(chanceFuseeBonus(10, 3) > chanceFuseeBonus(10, 2));
        CHECK(chanceFuseeBonus(10, 9) == Catch::Approx(chanceFuseeBonus(10, 3)));
        CHECK(chanceFuseeBonus(10, 3) < 1.0f);
    }

    SECTION("le multiplicateur ne fait jamais dépasser la certitude") {
        for (int kills = 1; kills <= 5; ++kills) {
            CHECK(chanceFuseeBonus(1, kills) <= 1.0f);
        }
    }

    SECTION("manche 0 ou négative : traitée comme la manche 1") {
        CHECK(chanceFuseeBonus(0, 1) == Catch::Approx(chanceFuseeBonus(1, 1)));
        CHECK(chanceFuseeBonus(-5, 1) == Catch::Approx(chanceFuseeBonus(1, 1)));
    }
}

/*
 * Le tirage décide aussi du cri : les morts d'une même explosion partent dans
 * zombieDeathBonusPositions si la fusée est partie, dans
 * zombieDeathSimplePositions sinon. Le piège serait d'en perdre ou d'en
 * dupliquer au découpage par explosion (voir CombatMode::update), d'où le
 * comptage image par image ci-dessous.
 */
TEST_CASE("Cris de mort : un par zombie tué, rangé selon la fusée", "[combat][bonus]") {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "artouste_cris_test";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "zombies.txt") << "40 0\n";

    CombatMode combat;
    combat.start(dir, solPlat);
    REQUIRE(combat.active());

    /* En vol stationnaire au-dessus de TOXIC_CEILING_M : hors de portée des
       pneus toxiques, sans quoi l'appareil est abattu en une dizaine de
       secondes et gameOver fige la mise à jour avant le premier kill. Nez
       basculé vers le bas (le canon tire selon +X du repère corps, voir
       fireDir) : les roquettes tombent à la verticale sur l'origine, là où la
       horde converge. */
    physics::RigidBody appareil;
    appareil.position    = vec3{0.0f, artouste::app::ZombieHorde::TOXIC_CEILING_M + 25.0f, 0.0f};
    appareil.orientation = glm::angleAxis(-artouste::HALF_PI, vec3{0.0f, 0.0f, 1.0f});

    int criscumules = 0;
    int avecFusee   = 0;
    int killsAvant  = 0;

    for (int image = 0; image < 60 * 90 && combat.kills() < 20; ++image) {
        combat.update(1.0f / 60.0f, appareil, true, solPlat);

        const CombatMode::SoundEvents& sons = combat.soundEvents();
        const int simples = static_cast<int>(sons.zombieDeathSimplePositions.size());
        const int bonus   = static_cast<int>(sons.zombieDeathBonusPositions.size());

        /* Exactement un cri par mort de cette image : rien de perdu au
           découpage, rien joué deux fois. */
        CHECK(simples + bonus == combat.kills() - killsAvant);

        killsAvant   = combat.kills();
        criscumules += simples + bonus;
        avecFusee   += bonus;
    }

    REQUIRE(combat.kills() > 0);
    CHECK(criscumules == combat.kills());
    /* Manche 1 : le tirage est généreux, des fusées doivent être parties. */
    CHECK(avecFusee > 0);
}
