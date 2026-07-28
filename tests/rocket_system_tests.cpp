/*
 * rocket_system_tests.cpp
 * Tests de l'explosion en zone du lance-roquettes (RocketSystem) : point
 * d'impact au sol, dégâts aux zombies dans le rayon d'effet, et forme de la
 * trace laissée au sol selon l'angle d'arrivée et la portée du tir. Se teste
 * sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/RocketSystem.hpp"
#include "app/combat/ZombieHorde.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using artouste::HALF_PI;
using artouste::vec3;
using artouste::app::RocketSystem;
using artouste::app::ZombieHorde;

TEST_CASE("RocketSystem : explosion au sol et dégâts de zone", "[combat][rocket]") {
    /* Sol plat au niveau de la mer : la roquette explose en franchissant y = 0. */
    const auto flat = [](float, float) { return 0.0f; };

    SECTION("une roquette tirée vers le sol tue les zombies dans le rayon") {
        ZombieHorde horde;
        horde.spawn(vec3{0.0f, 0.0f, 0.0f}); /* au point d'impact */
        horde.spawn(vec3{RocketSystem::EXPLOSION_RADIUS_M - 0.5f, 0.0f, 0.0f}); /* dans le rayon */
        horde.spawn(vec3{RocketSystem::EXPLOSION_RADIUS_M + 5.0f, 0.0f, 0.0f}); /* hors rayon */

        /* Tir droit vers le bas depuis 20 m au-dessus du zombie central : point
           d'impact déterministe (x = z = 0), indépendant de la vitesse. */
        RocketSystem rockets;
        const vec3 origin{0.0f, 20.0f, 0.0f};
        const vec3 dir{0.0f, -1.0f, 0.0f};
        rockets.spawn(origin, dir);

        /* Avance jusqu'à ce qu'une explosion se produise (borne de sécurité). */
        int explosions = 0;
        for (int i = 0; i < 2000 && explosions == 0; ++i) {
            explosions += rockets.update(0.01f, flat, horde).explosions;
        }
        CHECK(explosions == 1);

        /* Les deux premiers zombies (dans le rayon) sont touchés (Alive ->
           Dying), le troisième (hors rayon) reste intact. */
        CHECK(horde.zombies()[0].state != ZombieHorde::State::Alive);
        CHECK(horde.zombies()[1].state != ZombieHorde::State::Alive);
        CHECK(horde.zombies()[2].state == ZombieHorde::State::Alive);
    }
}

namespace {
/* Tire une roquette depuis 'origin' vers 'dir' et rend la trace au sol laissée
   par son explosion. Sol plat au niveau de la mer. */
RocketSystem::ScorchView tireEtReleveLaTrace(const vec3& origin, const vec3& dir) {
    const auto   flat = [](float, float) { return 0.0f; };
    RocketSystem rockets;
    ZombieHorde  horde;  /* arène vide : seule la trace nous intéresse ici */
    rockets.spawn(origin, dir);
    for (int i = 0; i < 4000 && rockets.scorches().empty(); ++i) {
        rockets.update(0.005f, flat, horde);
    }
    REQUIRE(rockets.scorches().size() == 1);
    return rockets.scorches()[0];
}
} /* namespace */

TEST_CASE("RocketSystem : forme et taille de la trace au sol", "[combat][rocket][scorch]") {
    SECTION("un tir à la verticale laisse une trace ronde") {
        const auto trace = tireEtReleveLaTrace(vec3{0.0f, 40.0f, 0.0f}, vec3{0.0f, -1.0f, 0.0f});
        CHECK(trace.elongation == Catch::Approx(1.0f));
    }

    SECTION("un tir rasant étire la trace le long de la trajectoire") {
        /* Presque à plat : la roquette retombe lentement et frappe de très loin
           sous une faible incidence. */
        const vec3 dir = glm::normalize(vec3{1.0f, -0.03f, 0.0f});
        const auto trace = tireEtReleveLaTrace(vec3{0.0f, 3.0f, 0.0f}, dir);
        CHECK(trace.elongation > 2.0f);
        /* Grand axe orienté vers +X, la direction du tir (yaw mesuré depuis +Z). */
        CHECK(trace.yaw == Catch::Approx(HALF_PI).margin(0.05));
    }

    SECTION("un tir lointain laisse une trace plus large qu'un tir à bout portant") {
        const auto proche =
            tireEtReleveLaTrace(vec3{0.0f, 12.0f, 0.0f}, glm::normalize(vec3{1.0f, -1.0f, 0.0f}));
        const auto lointain =
            tireEtReleveLaTrace(vec3{0.0f, 60.0f, 0.0f}, glm::normalize(vec3{1.0f, -0.15f, 0.0f}));
        CHECK(lointain.radius > proche.radius);
    }
}
