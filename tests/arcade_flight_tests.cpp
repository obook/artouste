/*
 * arcade_flight_tests.cpp
 * Tests du vol "arcade" (modèle simplifié). Ils s'exécutent sans aucun contexte
 * graphique : c'est ce qui permet de vérifier la physique seule, isolée du rendu.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "physics/ArcadeFlight.hpp"

#include <catch2/catch_test_macros.hpp>

using artouste::physics::ArcadeFlight;
using artouste::physics::Controls;

namespace {

/* Fait avancer la simulation pendant `seconds`, par petits pas de temps fixes. */
void advance(ArcadeFlight& flight, const Controls& controls, float seconds) {
    constexpr float step = 1.0f / 120.0f;
    for (float t = 0.0f; t < seconds; t += step) {
        flight.update(controls, step);
    }
}

}  /* namespace */

TEST_CASE("Le collectif au neutre maintient l'altitude", "[arcade]") {
    ArcadeFlight   flight;
    const Controls neutral;  /* collective = 0,5 par défaut */
    advance(flight, neutral, 5.0f);

    const auto& st = flight.state();
    REQUIRE(st.position.y < 0.01f);
    REQUIRE(st.position.y > -0.01f);
}

TEST_CASE("Collectif plein fait monter, collectif nul fait rester au sol", "[arcade]") {
    ArcadeFlight flight;

    Controls up;
    up.collective = 1.0f;
    advance(flight, up, 3.0f);
    REQUIRE(flight.state().position.y > 1.0f);

    /* Au sol, collectif à zéro : le garde-fou empêche de passer sous le terrain. */
    ArcadeFlight grounded;
    Controls     down;
    down.collective = 0.0f;
    advance(grounded, down, 3.0f);
    REQUIRE(grounded.state().position.y >= 0.0f);
}

TEST_CASE("Les palonniers font tourner le cap", "[arcade]") {
    ArcadeFlight flight;
    Controls     yaw;
    yaw.pedals = 1.0f;
    advance(flight, yaw, 1.0f);
    REQUIRE(flight.state().yaw > 0.5f);
}

TEST_CASE("Le reset ramène à l'état initial", "[arcade]") {
    ArcadeFlight flight;
    Controls     move;
    move.cyclicLongitudinal = 1.0f;
    move.collective         = 1.0f;
    advance(flight, move, 2.0f);
    REQUIRE(flight.state().position.y > 0.0f);

    flight.reset();
    const auto& st = flight.state();
    REQUIRE(st.position.x == 0.0f);
    REQUIRE(st.position.y == 0.0f);
    REQUIRE(st.position.z == 0.0f);
    REQUIRE(st.yaw == 0.0f);
}
