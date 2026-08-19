/*
 * projectile_system_tests.cpp
 * Tests des pneus toxiques lancés par les zombies (ProjectileSystem) :
 * trajectoire, impact sur l'appareil et expiration après une durée de vie
 * maximale. Se teste sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/ProjectileSystem.hpp"

#include <catch2/catch_test_macros.hpp>

using artouste::vec3;
using artouste::app::ProjectileSystem;

TEST_CASE("ProjectileSystem : trajectoire, impact et expiration", "[combat][projectile]") {
    ProjectileSystem projectiles;
    const vec3 heliCenter{0.0f, 5.0f, 0.0f};
    constexpr float heliRadius = 2.5f;

    SECTION("aucun projectile : aucun dégât") {
        const float damage = projectiles.update(0.1f, heliCenter, heliRadius);
        CHECK(damage == 0.0f);
        CHECK(projectiles.count() == 0);
    }

    SECTION("trajectoire vers la cible : touche l'appareil resté sur place") {
        projectiles.spawn(vec3{-30.0f, 0.0f, 0.0f}, heliCenter);
        REQUIRE(projectiles.count() == 1);

        float totalDamage = 0.0f;
        bool hit = false;
        for (int i = 0; i < 600 && !hit; ++i) { /* jusqu'à 5 s à 120 Hz, largement assez */
            const float damage = projectiles.update(1.0f / 120.0f, heliCenter, heliRadius);
            totalDamage += damage;
            if (damage > 0.0f) {
                hit = true;
            }
        }
        CHECK(hit);
        CHECK(totalDamage > 0.0f);
        CHECK(projectiles.count() == 0); /* retiré après impact */
    }

    SECTION("appareil loin de la trajectoire : pas d'impact, expire au bout d'un moment") {
        projectiles.spawn(vec3{-30.0f, 0.0f, 0.0f}, vec3{-30.0f, 0.0f, 500.0f});
        REQUIRE(projectiles.count() == 1);

        float totalDamage = 0.0f;
        for (int i = 0; i < 600; ++i) { /* 5 s : dépasse MAX_LIFETIME_S (4 s) */
            totalDamage += projectiles.update(1.0f / 120.0f, heliCenter, heliRadius);
        }
        CHECK(totalDamage == 0.0f);
        CHECK(projectiles.count() == 0); /* despawn de sécurité (durée de vie) */
    }

    SECTION("buildInstances reflète le nombre de projectiles actifs") {
        projectiles.spawn(vec3{0.0f, 0.0f, 0.0f}, vec3{10.0f, 0.0f, 0.0f});
        projectiles.spawn(vec3{0.0f, 0.0f, 0.0f}, vec3{-10.0f, 0.0f, 0.0f});
        CHECK(projectiles.buildInstances().size() == 2);
    }
}
