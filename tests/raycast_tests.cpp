/*
 * raycast_tests.cpp
 * Tests de l'intersection rayon/sphère (physics::raySphere), utilisée par le
 * mode zombie pour le tir à la volée (voir aussi weapon_tests.cpp). Se teste
 * sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "physics/Raycast.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using artouste::vec3;
using artouste::physics::raySphere;

TEST_CASE("raySphere : rayon droit vers une sphère", "[combat][raycast]") {
    const vec3 origin{0.0f, 0.0f, 0.0f};
    const vec3 dir{1.0f, 0.0f, 0.0f};

    SECTION("touche une sphère devant") {
        const auto hit = raySphere(origin, dir, vec3{10.0f, 0.0f, 0.0f}, 1.0f);
        REQUIRE(hit.hit);
        CHECK(hit.distance == Catch::Approx(9.0f));
    }

    SECTION("rate une sphère décalée hors du rayon") {
        const auto hit = raySphere(origin, dir, vec3{10.0f, 5.0f, 0.0f}, 1.0f);
        CHECK_FALSE(hit.hit);
    }

    SECTION("rate une sphère derrière l'origine") {
        const auto hit = raySphere(origin, dir, vec3{-10.0f, 0.0f, 0.0f}, 1.0f);
        CHECK_FALSE(hit.hit);
    }

    SECTION("touche une sphère tangente au bord du rayon") {
        const auto hit = raySphere(origin, dir, vec3{10.0f, 1.0f, 0.0f}, 1.0f);
        REQUIRE(hit.hit);
        CHECK(hit.distance == Catch::Approx(10.0f).margin(0.01f));
    }

    SECTION("origine à l'intérieur de la sphère : distance nulle ou positive") {
        const auto hit = raySphere(origin, dir, vec3{0.0f, 0.0f, 0.0f}, 5.0f);
        REQUIRE(hit.hit);
        CHECK(hit.distance >= 0.0f);
    }
}
