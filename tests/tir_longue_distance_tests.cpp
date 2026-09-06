/*
 * tir_longue_distance_tests.cpp
 * Tests de la prime de tir lointain du mode zombie : barème par paliers
 * (CombatMode::scoreDistance), libellé d'annonce correspondant
 * (annonceDistance) et remontée effective de la distance jusqu'au HUD. Se
 * teste sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : septembre 2026
 * Licence : GPL v2
 */

#include "app/combat/CombatMode.hpp"
#include "app/combat/ZombieHorde.hpp"
#include "physics/RigidBody.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace physics = artouste::physics;

using artouste::vec3;
using artouste::app::CombatMode;
using KillAnnouncement = CombatMode::KillAnnouncement;

namespace {

float solPlat(float, float) {
    return 0.0f;
}

} /* namespace */

TEST_CASE("Tir lointain : barème à trois paliers", "[combat][distance]") {
    SECTION("sous le premier palier, un tir ne rapporte aucune prime") {
        CHECK(CombatMode::scoreDistance(0.0f) == 0);
        CHECK(CombatMode::scoreDistance(CombatMode::TIR_LOIN_M - 0.1f) == 0);
    }

    SECTION("chaque palier est atteint dès la distance annoncée, pas après") {
        CHECK(CombatMode::scoreDistance(CombatMode::TIR_LOIN_M) == CombatMode::TIR_LOIN_SCORE);
        CHECK(CombatMode::scoreDistance(CombatMode::TIR_LONGUE_M) == CombatMode::TIR_LONGUE_SCORE);
        CHECK(CombatMode::scoreDistance(CombatMode::TIR_MAITRE_M) == CombatMode::TIR_MAITRE_SCORE);
    }

    SECTION("entre deux paliers, la prime ne bouge plus") {
        CHECK(CombatMode::scoreDistance(CombatMode::TIR_LONGUE_M - 0.1f)
              == CombatMode::TIR_LOIN_SCORE);
        CHECK(CombatMode::scoreDistance(CombatMode::TIR_MAITRE_M - 0.1f)
              == CombatMode::TIR_LONGUE_SCORE);
        CHECK(CombatMode::scoreDistance(5000.0f) == CombatMode::TIR_MAITRE_SCORE);
    }

    SECTION("les paliers montent, en distance comme en points") {
        CHECK(CombatMode::TIR_LOIN_M < CombatMode::TIR_LONGUE_M);
        CHECK(CombatMode::TIR_LONGUE_M < CombatMode::TIR_MAITRE_M);
        CHECK(CombatMode::TIR_LOIN_SCORE < CombatMode::TIR_LONGUE_SCORE);
        CHECK(CombatMode::TIR_LONGUE_SCORE < CombatMode::TIR_MAITRE_SCORE);
    }

    SECTION("annonce et prime tombent aux mêmes seuils") {
        for (const float m : {0.0f, 149.0f, 150.0f, 299.0f, 300.0f, 399.0f, 400.0f, 900.0f}) {
            const bool prime   = CombatMode::scoreDistance(m) > 0;
            const bool annonce = CombatMode::annonceDistance(m) != KillAnnouncement::None;
            CHECK(prime == annonce);
        }
        CHECK(CombatMode::annonceDistance(CombatMode::TIR_LOIN_M) == KillAnnouncement::Loin);
        CHECK(CombatMode::annonceDistance(CombatMode::TIR_LONGUE_M)
              == KillAnnouncement::LongueDistance);
        CHECK(CombatMode::annonceDistance(CombatMode::TIR_MAITRE_M) == KillAnnouncement::Maitre);
    }
}

/*
 * La prime doit vraiment atteindre le HUD : l'appareil tire à la verticale
 * depuis 500 m (hors de portée des pneus toxiques, sinon il est abattu avant le
 * premier kill), donc chaque roquette parcourt bien plus que TIR_MAITRE_M.
 */
TEST_CASE("Tir lointain : la distance remonte jusqu'à l'annonce", "[combat][distance]") {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "artouste_portee_test";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "zombies.txt") << "40 0\n";

    CombatMode combat;
    combat.start(dir, solPlat);
    REQUIRE(combat.active());

    physics::RigidBody appareil;
    appareil.position    = vec3{0.0f, 500.0f, 0.0f};
    appareil.orientation = glm::angleAxis(-artouste::HALF_PI, vec3{0.0f, 0.0f, 1.0f});

    bool annonceVue = false;
    for (int image = 0; image < 60 * 120 && !annonceVue; ++image) {
        combat.update(1.0f / 60.0f, appareil, true, solPlat);
        if (combat.killAnnouncement() != KillAnnouncement::None) {
            annonceVue = true;
            /* Tir plongeant de 500 m : dernier palier atteint, et la mention
               de distance accompagne l'annonce quel qu'en soit le libellé (un
               kill multiple garde le sien). */
            CHECK(combat.killAnnounceDistanceM() >= CombatMode::TIR_MAITRE_M);
        }
    }

    REQUIRE(combat.kills() > 0);
    CHECK(annonceVue);
    /* Chaque kill rapporte au moins 25 points, et la prime de distance s'y
       ajoute : le score dépasse forcément le simple comptage des morts. */
    CHECK(combat.score() > 25 * combat.kills());
}

/*
 * Cumul : la prime de distance s'AJOUTE au score du kill multiple, elle ne le
 * remplace pas. Vérifié sur le score réel, pas sur la lecture du code : on tire
 * de 500 m (tout impact dépasse donc TIR_MAITRE_M) sur une horde qui converge
 * vers un même point, et on relève image par image ce que chaque explosion
 * rapporte.
 */
TEST_CASE("Tir lointain : la prime se cumule avec le kill multiple", "[combat][distance]") {
    /* Barème des kills multiples, tel que documenté (voir killScoreForCount) :
       c'est la référence à laquelle le score constaté doit correspondre, prime
       de distance en plus. */
    const auto scoreKills = [](int k) { return k == 0 ? 0 : k == 1 ? 25 : k == 2 ? 75 : 125; };

    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "artouste_cumul_test";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "zombies.txt") << "40 0\n";

    CombatMode combat;
    combat.start(dir, solPlat);
    REQUIRE(combat.active());

    physics::RigidBody appareil;
    appareil.position    = vec3{0.0f, 500.0f, 0.0f};
    appareil.orientation = glm::angleAxis(-artouste::HALF_PI, vec3{0.0f, 0.0f, 1.0f});

    int scoreAvant = 0;
    int killsAvant = 0;
    int multiplesVus = 0;
    int simplesVus   = 0;

    for (int image = 0; image < 60 * 180 && multiplesVus < 3; ++image) {
        combat.update(1.0f / 60.0f, appareil, true, solPlat);

        const int deltaKills = combat.kills() - killsAvant;
        const int deltaScore = combat.score() - scoreAvant;
        killsAvant = combat.kills();
        scoreAvant = combat.score();

        /* Une seule explosion sur l'image : deltaKills est alors le nombre de
           zombies fauchés par CETTE explosion, et deltaScore ce qu'elle a
           rapporté en tout. Plusieurs explosions mélangeraient les comptes. */
        if (deltaKills <= 0 || combat.soundEvents().explosionPositions.size() != 1) {
            continue;
        }

        /* Tir plongeant de 500 m : dernier palier, quoi qu'il arrive. */
        CHECK(deltaScore == scoreKills(deltaKills) + CombatMode::TIR_MAITRE_SCORE);

        if (deltaKills >= 2) {
            ++multiplesVus;
            /* Le libellé reste celui du kill multiple, et la distance
               l'accompagne : les deux exploits sont annoncés ensemble. */
            const KillAnnouncement ann = combat.killAnnouncement();
            CHECK((ann == KillAnnouncement::Double || ann == KillAnnouncement::Triple
                   || ann == KillAnnouncement::Carnage));
            CHECK(combat.killAnnounceDistanceM() >= CombatMode::TIR_MAITRE_M);
        } else {
            ++simplesVus;
            /* Kill simple de loin : faute de kill multiple à annoncer, c'est le
               tir qui prend le bandeau. */
            CHECK(combat.killAnnouncement() == KillAnnouncement::Maitre);
        }
    }

    /* La preuve ne vaut que si les deux cas se sont réellement produits. */
    CHECK(simplesVus > 0);
    CHECK(multiplesVus > 0);
}
