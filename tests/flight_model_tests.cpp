// Tests du modèle de vol M3. Comme physics/, ils s'exécutent sans contexte
// graphique. On vérifie les comportements clés : sustentation, décollage,
// stabilité en assiette, anti-couple, et absence de NaN sous entrées aléatoires.

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

// Inclinaison de l'axe rotor par rapport à la verticale (radians).
float tiltAngle(const FlightModel& model) {
    const artouste::vec3 up = model.body().orientation * artouste::vec3{0.0f, 1.0f, 0.0f};
    return std::acos(artouste::clamp(up.y, -1.0f, 1.0f));
}

}  // namespace

TEST_CASE("Hors effet de sol, le collectif de sustentation tient l'altitude", "[flight]") {
    FlightModel model;
    model.reset(50.0f);  // assez haut pour ignorer l'effet de sol
    Controls hover;
    hover.collective = artouste::physics::COLL_HOVER;
    advance(model, hover, 5.0f);

    // L'appareil ne doit ni s'enfoncer ni grimper notablement.
    REQUIRE(std::fabs(model.body().position.y - 50.0f) < 1.0f);
    REQUIRE(std::fabs(model.body().velocity.y) < 1.0f);
}

TEST_CASE("L'effet de sol soulève au ras du sol", "[flight][m5]") {
    FlightModel model;  // au sol (y = 0)
    Controls    hover;
    hover.collective = artouste::physics::COLL_HOVER;
    advance(model, hover, 4.0f);

    // Au même collectif de sustentation, le coussin d'air fait décoller.
    REQUIRE(model.body().position.y > 0.5f);
}

TEST_CASE("L'effet de translation augmente la poussée", "[flight][m5]") {
    FlightModel model;
    model.reset(50.0f);
    Controls hover;
    hover.collective = artouste::physics::COLL_HOVER;
    advance(model, hover, 2.0f);
    const float hoverThrust = model.lastThrust();

    // On prend de la vitesse vers l'avant.
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
    Controls    climb;
    climb.collective = 1.0f;
    advance(model, climb, 3.0f);

    REQUIRE(model.body().position.y > 2.0f);
    REQUIRE(model.body().velocity.y > 0.0f);
}

TEST_CASE("Sans poussée, l'appareil reste au sol (garde-fou)", "[flight]") {
    FlightModel model;
    Controls    idle;
    idle.collective = 0.0f;
    advance(model, idle, 3.0f);

    REQUIRE(model.body().position.y >= 0.0f);
}

TEST_CASE("La stabilité augmentée ramène l'assiette à plat", "[flight]") {
    FlightModel model;

    // On incline en commandant le cyclique pendant un temps...
    Controls roll;
    roll.collective   = artouste::physics::COLL_HOVER;
    roll.cyclicLateral = 1.0f;
    advance(model, roll, 1.0f);
    const float tilted = tiltAngle(model);
    REQUIRE(tilted > 0.05f);

    // ...puis on relâche : l'assiette doit revenir vers l'horizontale.
    Controls release;
    release.collective = artouste::physics::COLL_HOVER;
    advance(model, release, 4.0f);
    REQUIRE(tiltAngle(model) < tilted * 0.5f);
}

TEST_CASE("Les palonniers commandent le lacet", "[flight]") {
    FlightModel model;
    Controls    yaw;
    yaw.collective = artouste::physics::COLL_HOVER;
    yaw.pedals     = 1.0f;
    advance(model, yaw, 1.0f);
    REQUIRE(std::fabs(model.body().angularVelocity.y) > 0.1f);
}

TEST_CASE("Aucun NaN ni Inf sur entrées aléatoires bornées", "[flight][fuzz]") {
    FlightModel model;

    // Générateur déterministe simple (pas de dépendance à <random> ni au temps).
    unsigned int seed = 12345u;
    auto         next = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);  // [0,1)
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
