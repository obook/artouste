/*
 * flight_model_tests.cpp
 * Tests du modèle de vol réaliste. Comme la physique arcade, ils tournent sans
 * contexte graphique. On vérifie les comportements clés : sustentation, décollage,
 * stabilité d'assiette, anti-couple, et absence de NaN sous entrées aléatoires.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "physics/FlightModel.hpp"
#include "physics/constants.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using artouste::physics::Controls;
using artouste::physics::FlightModel;

namespace {

constexpr float SIM_DT = 1.0f / 240.0f;

void advance(FlightModel& model, const Controls& controls, float seconds) {
    const int steps = static_cast<int>(seconds / SIM_DT);
    for (int i = 0; i < steps; ++i) {
        model.update(controls, SIM_DT);
    }
}

/* Inclinaison de l'axe du rotor par rapport à la verticale, en radians. */
float tiltAngle(const FlightModel& model) {
    const artouste::vec3 up = model.body().orientation * artouste::vec3{0.0f, 1.0f, 0.0f};
    return std::acos(artouste::clamp(up.y, -1.0f, 1.0f));
}

}  /* namespace */

TEST_CASE("Hors effet de sol, le collectif de sustentation tient l'altitude", "[flight]") {
    FlightModel model;
    model.reset(50.0f);  /* assez haut pour ignorer l'effet de sol */
    model.turbine().forceRunning();  /* rotor en régime, sinon pas de portance */
    Controls hover;
    hover.collective = artouste::physics::COLL_HOVER;
    advance(model, hover, 5.0f);

    /* L'appareil ne doit ni s'enfoncer ni grimper de façon notable. */
    REQUIRE(std::fabs(model.body().position.y - 50.0f) < 1.0f);
    REQUIRE(std::fabs(model.body().velocity.y) < 1.0f);
}

TEST_CASE("L'effet de sol soulève au ras du sol", "[flight][m5]") {
    FlightModel model;  /* au sol (y = 0) */
    model.turbine().forceRunning();
    Controls    hover;
    hover.collective = artouste::physics::COLL_HOVER;
    advance(model, hover, 4.0f);

    /* Au même collectif de sustentation, le coussin d'air fait décoller. */
    REQUIRE(model.body().position.y > 0.5f);
}

TEST_CASE("L'effet de translation augmente la poussée", "[flight][m5]") {
    FlightModel model;
    model.reset(50.0f);
    model.turbine().forceRunning();
    Controls hover;
    hover.collective = artouste::physics::COLL_HOVER;
    advance(model, hover, 2.0f);
    const float hoverThrust = model.lastThrust();

    /* On prend de la vitesse vers l'avant. */
    Controls forward = hover;
    forward.cyclicLongitudinal = 1.0f;
    advance(model, forward, 6.0f);

    const auto& v             = model.body().velocity;
    const float airspeed      = std::sqrt(v.x * v.x + v.z * v.z);
    REQUIRE(airspeed > artouste::physics::ETL_V_HIGH);
    REQUIRE(model.lastThrust() > hoverThrust * 1.02f);
}

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

TEST_CASE("Turbine coupée, plein collectif ne décolle pas", "[flight][turbine]") {
    FlightModel model;  /* turbine à l'arrêt par défaut au lancement */
    REQUIRE(model.turbine().state() == artouste::physics::Turbine::State::Arret);

    Controls climb;
    climb.collective = 1.0f;
    advance(model, climb, 3.0f);

    /* Sans rotor entraîné, aucune portance : l'appareil reste posé. */
    REQUIRE(model.body().position.y < 0.001f);
}

TEST_CASE("Le rotor attend le plein régime de la turbine", "[flight][turbine]") {
    using State = artouste::physics::Turbine::State;
    FlightModel    model;
    const Controls idle;  /* on n'observe que la turbine */

    /* Démarrage : la turbine monte d'abord seule, rotor encore immobile. */
    model.turbine().toggle();
    REQUIRE(model.turbine().state() == State::Demarrage);
    advance(model, idle, artouste::physics::TURBINE_START_TIME * 0.5f);
    REQUIRE(model.turbine().state() == State::Demarrage);
    REQUIRE(model.turbine().rotorFraction() == 0.0f);  /* pales encore à l'arrêt */
    REQUIRE(model.turbine().turbineFraction() > 0.0f); /* turbine en train de monter */

    /* La turbine atteint son plein régime, mais le frein rotor est encore serré :
       les pales restent immobiles le temps que le pilote le lâche (état Attente).
       On s'arrête au milieu du délai de frein pour observer cet état, quelles que
       soient les durées choisies pour la montée et le délai. */
    advance(model, idle,
            artouste::physics::TURBINE_START_TIME * 0.5f +
                artouste::physics::ROTOR_BRAKE_DELAY * 0.5f);
    REQUIRE(model.turbine().state() == State::Attente);
    REQUIRE(model.turbine().turbineFraction() == 1.0f);
    REQUIRE(model.turbine().rotorFraction() == 0.0f);  /* pales toujours à l'arrêt */

    /* Frein lâché après le délai : le rotor s'accouple puis atteint son régime. */
    advance(model, idle, artouste::physics::ROTOR_BRAKE_DELAY);
    REQUIRE(model.turbine().state() == State::Embrayage);
    advance(model, idle, artouste::physics::ROTOR_ENGAGE_TIME + 1.0f);
    REQUIRE(model.turbine().state() == State::Regime);
    REQUIRE(model.turbine().rotorFraction() == 1.0f);

    /* Arrêt : après le temps de descente, le rotor est immobile. */
    model.turbine().toggle();
    REQUIRE(model.turbine().state() == State::Extinction);
    advance(model, idle, artouste::physics::TURBINE_STOP_TIME + 1.0f);
    REQUIRE(model.turbine().state() == State::Arret);
    REQUIRE(model.turbine().rotorFraction() == 0.0f);
}
