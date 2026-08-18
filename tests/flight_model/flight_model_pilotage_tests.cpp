/*
 * flight_model_pilotage_tests.cpp
 * Décollage, tenue au sol, assiette, lacet, virage coordonné et absence de NaN.
 *
 * Tournent sans contexte graphique. Outils communs dans aide_vol.hpp.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "flight_model/aide_vol.hpp"

#include "physics/constants.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using artouste::physics::Controls;
using artouste::physics::FlightModel;
using essais_vol::advance;
using essais_vol::SIM_DT;
using essais_vol::tiltAngle;

TEST_CASE("Collectif plein fait décoller", "[flight]") {
    FlightModel model;
    model.turbine().forceRunning();
    Controls    climb;
    climb.collective = 1.0f;
    advance(model, climb, 3.0f);

    REQUIRE(model.body().position.y > 2.0f);
    REQUIRE(model.body().velocity.y > 0.0f);
}

TEST_CASE("Sans poussée, l'appareil reste au sol (garde-fou)", "[flight]") {
    FlightModel model;
    model.turbine().forceRunning();  /* turbine lancée, mais collectif nul */
    Controls    idle;
    idle.collective = 0.0f;
    advance(model, idle, 3.0f);

    REQUIRE(model.body().position.y >= 0.0f);
}

TEST_CASE("Au sol collectif à zéro, l'appareil reste immobile", "[flight]") {
    FlightModel    model;
    const Controls rest;  /* collectif = 0 par défaut au lancement */
    REQUIRE(rest.collective == 0.0f);

    /* Même avec une commande de palonnier, les patins restent collés au sol. */
    Controls input = rest;
    input.pedals   = 1.0f;
    advance(model, input, 3.0f);

    const auto& b = model.body();
    REQUIRE(std::fabs(b.position.x) < 0.001f);
    REQUIRE(std::fabs(b.position.y) < 0.001f);
    REQUIRE(std::fabs(b.position.z) < 0.001f);
    REQUIRE(std::fabs(b.angularVelocity.y) < 0.001f);
}

TEST_CASE("La stabilité augmentée ramène l'assiette à plat", "[flight]") {
    FlightModel model;
    model.turbine().forceRunning();

    /* On incline l'appareil en poussant le cyclique pendant un moment... */
    Controls roll;
    roll.collective   = artouste::physics::COLL_HOVER;
    roll.cyclicLateral = 1.0f;
    advance(model, roll, 1.0f);
    const float tilted = tiltAngle(model);
    REQUIRE(tilted > 0.05f);

    /* ...puis on relâche : l'assiette doit revenir vers l'horizontale. */
    Controls release;
    release.collective = artouste::physics::COLL_HOVER;
    advance(model, release, 4.0f);
    REQUIRE(tiltAngle(model) < tilted * 0.5f);
}

TEST_CASE("Les palonniers commandent le lacet", "[flight]") {
    FlightModel model;
    model.turbine().forceRunning();
    Controls    yaw;
    yaw.collective = artouste::physics::COLL_HOVER;
    yaw.pedals     = 1.0f;
    advance(model, yaw, 1.0f);
    REQUIRE(std::fabs(model.body().angularVelocity.y) > 0.1f);
}

TEST_CASE("Le virage coordonné tourne le nez avec la vitesse, pas au stationnaire",
          "[flight]") {
    Controls hover;
    hover.collective = artouste::physics::COLL_HOVER;

    /* Stationnaire : le cyclique latéral incline seulement, ne tourne pas le nez
       (retour d'un pilote réel : sinon virer sans vitesse serait déjà "coordonné",
       alors que l'appareil doit partir en crabe, utile au posé). */
    FlightModel hoverModel;
    hoverModel.reset(50.0f);
    hoverModel.turbine().forceRunning();
    Controls bank = hover;
    bank.cyclicLateral = 1.0f;
    advance(hoverModel, bank, 1.0f);
    REQUIRE(std::fabs(hoverModel.body().angularVelocity.y) < 0.02f);

    /* Avec de la vitesse établie (même prise de vitesse que le test de l'ETL),
       le même cyclique latéral doit maintenant tourner le nez, dans le sens de
       l'inclinaison (comme un avion qui vire en inclinant). */
    FlightModel cruiseModel;
    cruiseModel.reset(50.0f);
    cruiseModel.turbine().forceRunning();
    Controls forward = hover;
    forward.cyclicLongitudinal = 1.0f;
    advance(cruiseModel, forward, 6.0f);
    Controls bankAtSpeed = forward;
    bankAtSpeed.cyclicLateral = 1.0f;
    advance(cruiseModel, bankAtSpeed, 1.0f);
    REQUIRE(std::fabs(cruiseModel.body().angularVelocity.y) > 0.05f);
}

TEST_CASE("Aucun NaN ni Inf sur entrées aléatoires bornées", "[flight][fuzz]") {
    FlightModel model;
    model.turbine().forceRunning();

    /* Générateur pseudo-aléatoire déterministe et simple : pas de dépendance
       à <random> ni à l'horloge, donc le test donne toujours le même résultat. */
    unsigned int seed = 12345u;
    auto         next = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);  /* dans [0, 1) */
    };

    for (int i = 0; i < 10000; ++i) {
        Controls c;
        c.cyclicLateral      = next() * 2.0f - 1.0f;
        c.cyclicLongitudinal = next() * 2.0f - 1.0f;
        c.collective         = next();
        c.pedals             = next() * 2.0f - 1.0f;
        model.update(c, SIM_DT);
    }

    const auto& b = model.body();
    REQUIRE(std::isfinite(b.position.x));
    REQUIRE(std::isfinite(b.position.y));
    REQUIRE(std::isfinite(b.position.z));
    REQUIRE(std::isfinite(b.velocity.x));
    REQUIRE(std::isfinite(b.orientation.w));
    REQUIRE(std::isfinite(b.angularVelocity.y));
}
