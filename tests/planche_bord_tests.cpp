/*
 * planche_bord_tests.cpp
 * Tests des échelles des cadrans dessinés de la planche 3dexport : une valeur
 * devient un angle depuis midi, sens horaire vu du pilote.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#include "render/PlancheBord.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace artouste::render;
using Catch::Approx;

TEST_CASE("Une valeur hors échelle reste en butée", "[planche]") {
    CHECK(angleSur(ECHELLE_TURBINE, -500.0f) == Approx(-135.0f));
    CHECK(angleSur(ECHELLE_TURBINE, 99000.0f) == Approx(135.0f));
}

TEST_CASE("Tachymètre double : régimes nominaux", "[planche]") {
    /* 34 000 tr/min turbine : 85 % de l'échelle de 270 deg. */
    CHECK(angleSur(ECHELLE_TURBINE, 34000.0f) == Approx(94.5f));
    /* 360 tr/min rotor sur 0-420. */
    CHECK(angleSur(ECHELLE_ROTOR, 360.0f) == Approx(-135.0f + 270.0f * 360.0f / 420.0f));
}

TEST_CASE("Unité triple : la pression d'huile tourne dans l'autre sens", "[planche]") {
    CHECK(angleSur(ECHELLE_HUILE_BAR, 0.0f) == Approx(100.0f));
    CHECK(angleSur(ECHELLE_HUILE_BAR, 0.8f) == Approx(15.0f));
    CHECK(angleSur(ECHELLE_HUILE_C, 0.0f) == Approx(-100.0f));
    CHECK(angleSur(ECHELLE_TUYERE, 400.0f) == Approx(0.0f));
}

TEST_CASE("Pas, carburant et volts : midi au milieu de l'échelle", "[planche]") {
    CHECK(angleSur(ECHELLE_PAS, 10.0f) == Approx(0.0f));
    CHECK(angleSur(ECHELLE_CARBURANT, 75.0f) == Approx(0.0f));
    CHECK(angleSur(ECHELLE_VOLTS, 15.0f) == Approx(0.0f));
}

TEST_CASE("Bille-aiguille et thermomètre : virage au taux standard, air à mi-échelle", "[planche]") {
    /* Virage au taux standard (3 deg/s, demi-tour en une minute) : repère à 25 deg. */
    CHECK(angleSur(ECHELLE_VIRAGE, 3.0f) == Approx(25.0f));
    CHECK(angleSur(ECHELLE_VIRAGE, -3.0f) == Approx(-25.0f));
    CHECK(angleSur(ECHELLE_AIR, 10.0f) == Approx(0.0f));
}

TEST_CASE("Thermomètre extérieur : atmosphère type", "[planche]") {
    CHECK(temperatureAir(0.0f) == Approx(15.0f));
    CHECK(temperatureAir(2000.0f) == Approx(2.0f));
}

TEST_CASE("Carburant : les litres du simulateur en gallons US", "[planche]") {
    CHECK(gallonsUs(378.5f) == Approx(100.0f));
}
