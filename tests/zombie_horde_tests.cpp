/*
 * zombie_horde_tests.cpp
 * Tests du cycle de vie d'un zombie (ZombieHorde) : dégâts, mort et despawn,
 * puis marche vers le joueur et jets de boulettes toxiques. Se teste sans
 * contexte graphique (ni render::Zombies ni CombatMode ne sont nécessaires).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/ZombieHorde.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using artouste::vec3;
using artouste::app::ZombieHorde;

namespace {
/* Terrain plat à l'altitude 0, pour isoler la logique de tir/dégâts du
   recalage de relief (déjà couvert ailleurs, voir ZombieHorde::update). */
float flatGround(float, float) noexcept {
    return 0.0f;
}
} /* namespace */

TEST_CASE("ZombieHorde : dégâts, mort et despawn", "[combat][zombie]") {
    ZombieHorde horde;
    horde.spawn(vec3{0.0f, 0.0f, 0.0f});
    REQUIRE(horde.count() == 1);

    SECTION("un coup sous-létal blesse sans tuer") {
        horde.applyDamage(0, 10.0f);
        CHECK(horde.zombies()[0].health == Catch::Approx(90.0f));
        CHECK(horde.zombies()[0].state == ZombieHorde::State::Alive);
        CHECK(horde.buildInstanceMatrices().size() == 1);
    }

    SECTION("un coup létal passe en Dying puis despawn après l'anim de chute") {
        horde.applyDamage(0, 1000.0f);
        CHECK(horde.zombies()[0].state == ZombieHorde::State::Dying);
        CHECK(horde.buildInstanceMatrices().size() == 1); /* encore dessiné en train de tomber */

        /* Joueur loin et hors de portée : sans incidence sur un zombie Dying
           (il ne marche ni ne lance plus, voir update). */
        const vec3 farPlayer{5000.0f, 100.0f, 5000.0f};

        /* Avant la fin de l'anim de chute : toujours présent. */
        horde.update(0.5f, farPlayer, 100.0f, 1.0f, flatGround);
        CHECK(horde.count() == 1);

        /* Après (durée totale > 1 s, voir DEATH_ANIM_DURATION_S) : despawn. */
        horde.update(0.6f, farPlayer, 100.0f, 1.0f, flatGround);
        CHECK(horde.count() == 0);
        CHECK(horde.buildInstanceMatrices().empty());
    }

    SECTION("un zombie déjà mort n'encaisse plus de dégâts") {
        horde.applyDamage(0, 1000.0f); /* Dying */
        horde.applyDamage(0, 1000.0f); /* ne doit rien changer d'autre */
        CHECK(horde.zombies()[0].state == ZombieHorde::State::Dying);
    }

    SECTION("indice hors bornes : sans effet") {
        horde.applyDamage(42, 10.0f);
        CHECK(horde.zombies()[0].health == Catch::Approx(100.0f));
    }
}

TEST_CASE("ZombieHorde : marche vers le joueur et jets de boulettes toxiques",
          "[combat][zombie][ai]") {
    ZombieHorde horde;
    horde.spawn(vec3{100.0f, 0.0f, 0.0f}); /* loin sur l'axe X */
    const vec3 player{0.0f, 0.0f, 0.0f};

    SECTION("un zombie vivant avance vers le joueur") {
        const float distAvant = horde.zombies()[0].position.x;
        horde.update(
            1.0f, player, 100.0f /* AGL au-dessus du plafond : pas de jet */, 1.0f, flatGround);
        CHECK(horde.zombies()[0].position.x < distAvant);
        CHECK(horde.zombies()[0].position.x > 0.0f); /* pas de dépassement du joueur */
    }

    SECTION("le facteur de vitesse accélère la marche") {
        ZombieHorde lent;
        lent.spawn(vec3{100.0f, 0.0f, 0.0f});
        ZombieHorde rapide;
        rapide.spawn(vec3{100.0f, 0.0f, 0.0f});

        lent.update(1.0f, player, 100.0f, 1.0f, flatGround);
        rapide.update(1.0f, player, 100.0f, 2.0f, flatGround);
        CHECK(rapide.zombies()[0].position.x < lent.zombies()[0].position.x);
    }

    SECTION("hors de portée : marche mais ne lance pas") {
        horde.zombies()[0].position.x = 200.0f; /* > TOXIC_RANGE_MAX_M (60 m) */
        const auto requests =
            horde.update(0.1f, player, 0.0f /* sous le plafond */, 1.0f, flatGround);
        CHECK(requests.empty());
    }

    SECTION("au-dessus du plafond d'altitude : à portée mais ne lance pas") {
        const auto requests =
            horde.update(0.1f, vec3{95.0f, 0.0f, 0.0f}, 100.0f /* > 35 m */, 1.0f, flatGround);
        CHECK(requests.empty());
    }

    SECTION("à portée, sous le plafond, hors cooldown : lance") {
        const auto requests = horde.update(0.1f, vec3{95.0f, 0.0f, 0.0f}, 0.0f, 1.0f, flatGround);
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].target.x == Catch::Approx(95.0f));
        /* Le cooldown est réarmé : un appel immédiat suivant ne relance pas. */
        const auto requests2 = horde.update(0.01f, vec3{95.0f, 0.0f, 0.0f}, 0.0f, 1.0f, flatGround);
        CHECK(requests2.empty());
    }
}
