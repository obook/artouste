/*
 * rocket_system_tests.cpp
 * Tests de l'explosion en zone du lance-roquettes (RocketSystem) : point
 * d'impact au sol et dégâts aux zombies dans le rayon d'effet. Se teste sans
 * contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/RocketSystem.hpp"
#include "app/combat/ZombieHorde.hpp"

#include <catch2/catch_test_macros.hpp>

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
