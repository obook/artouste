/*
 * placement_cadran_tests.cpp
 * Tests du placement d'un instrument FlightGear sur un cadran de planche
 * inclinée (poste 3dexport).
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#include "render/PlacementCadran.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using artouste::vec3;
using artouste::vec4;
using artouste::mat4;
using artouste::render::placementCadran;
using Catch::Approx;

namespace {

const vec3 NORMALE_PLANCHE = glm::normalize(vec3{0.948f, 0.318f, 0.0f});

}  /* namespace */

TEST_CASE("Planche verticale, sans échelle ni avance : identité", "[cadran]") {
    const mat4 m = placementCadran(vec3{0.0f}, vec3{1.0f, 0.0f, 0.0f}, 1.0f, 0.0f);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            CHECK(m[c][r] == Approx(c == r ? 1.0f : 0.0f).margin(1e-6));
        }
    }
}

TEST_CASE("L'axe X de l'instrument suit la normale, à l'échelle", "[cadran]") {
    const mat4 m = placementCadran(vec3{0.0f}, NORMALE_PLANCHE, 0.79f, 0.0f);
    const vec3 x = vec3(m * vec4(1.0f, 0.0f, 0.0f, 0.0f));
    CHECK(glm::length(x) == Approx(0.79f));
    CHECK(glm::dot(glm::normalize(x), NORMALE_PLANCHE) == Approx(1.0f));
}

TEST_CASE("Le haut de l'instrument reste vers le haut, dans le plan de la planche", "[cadran]") {
    const mat4 m = placementCadran(vec3{0.0f}, NORMALE_PLANCHE, 1.0f, 0.0f);
    const vec3 y = vec3(m * vec4(0.0f, 1.0f, 0.0f, 0.0f));
    CHECK(y.y > 0.9f);
    CHECK(glm::dot(y, NORMALE_PLANCHE) == Approx(0.0f).margin(1e-6));
    /* Pas de vrille : le haut reste dans le plan vertical de la normale. */
    CHECK(y.z == Approx(0.0f).margin(1e-6));
}

TEST_CASE("L'origine est au centre du cadran, avancée le long de la normale", "[cadran]") {
    const vec3 centre{-4.4146f, -0.1517f, 0.1411f};
    const mat4 m = placementCadran(centre, NORMALE_PLANCHE, 0.79f, 0.01f);
    const vec3 o = vec3(m * vec4(0.0f, 0.0f, 0.0f, 1.0f));
    const vec3 attendu = centre + NORMALE_PLANCHE * 0.01f;
    CHECK(o.x == Approx(attendu.x));
    CHECK(o.y == Approx(attendu.y));
    CHECK(o.z == Approx(attendu.z));
}
