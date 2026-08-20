/*
 * bille_tests.cpp
 * Aiguille-bille : force spécifique latérale (physics::billeG) et taux de virage
 * (physics::tauxVirageDegS). Se teste sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "physics/FlightModel.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using artouste::physics::billeG;
using artouste::physics::tauxVirageDegS;
using artouste::physics::RigidBody;
using artouste::vec3;

namespace {

/* Roulis autour de l'axe longitudinal (X du repère corps), positif à droite. */
artouste::quat roulis(float rad) {
    return artouste::quat(std::cos(rad * 0.5f), std::sin(rad * 0.5f), 0.0f, 0.0f);
}

}  /* namespace anonyme */

TEST_CASE("Bille centrée en vol droit stabilisé", "[bille]") {
    RigidBody body;
    body.velocity = vec3{40.0f, 0.0f, 0.0f};
    REQUIRE(billeG(body) == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("Bille du côté bas en roulis sans virage", "[bille]") {
    RigidBody body;
    body.orientation = roulis(0.3f);  /* incliné à droite */
    REQUIRE(billeG(body) > 0.1f);
    body.orientation = roulis(-0.3f); /* incliné à gauche */
    REQUIRE(billeG(body) < -0.1f);
}

TEST_CASE("Bille à l'extérieur du virage à plat", "[bille]") {
    RigidBody body;
    body.velocity        = vec3{40.0f, 0.0f, 0.0f};  /* cap vers l'est (X monde) */
    body.angularVelocity = vec3{0.0f, -0.2f, 0.0f};  /* lacet à droite (Y monde vers le haut) */
    REQUIRE(billeG(body) < -0.1f);                   /* bille à gauche : dérapage */
}

TEST_CASE("Bille centrée en virage coordonné", "[bille]") {
    /* Condition de coordination : tan(roulis) = V * omega / g. */
    RigidBody   body;
    const float v     = 40.0f;
    const float omega = 0.2f;
    const float phi   = std::atan(v * omega / artouste::physics::G);
    body.orientation     = roulis(phi);
    body.velocity        = vec3{v, 0.0f, 0.0f};
    body.angularVelocity = glm::conjugate(body.orientation) * vec3{0.0f, -omega, 0.0f};
    REQUIRE(billeG(body) == Catch::Approx(0.0f).margin(0.02f));
}

TEST_CASE("Taux de virage positif à droite", "[bille]") {
    RigidBody body;
    /* Le repère monde a Y vers le haut et Z vers le sud : un virage à droite est une
       rotation négative autour de Y. */
    body.angularVelocity = vec3{0.0f, -0.2f, 0.0f};
    REQUIRE(tauxVirageDegS(body) == Catch::Approx(glm::degrees(0.2f)));
    body.angularVelocity = vec3{0.0f, 0.2f, 0.0f};
    REQUIRE(tauxVirageDegS(body) == Catch::Approx(-glm::degrees(0.2f)));
}

TEST_CASE("Lacet fuselage sur la tranche ne fait pas virer", "[bille]") {
    /* Roulis de 90 degrés : l'axe de lacet du fuselage est horizontal, la rotation
       autour de lui ne change pas le cap. */
    RigidBody body;
    body.orientation     = roulis(1.5707963f);
    body.angularVelocity = vec3{0.0f, -0.2f, 0.0f};
    REQUIRE(tauxVirageDegS(body) == Catch::Approx(0.0f).margin(1e-4f));
}
