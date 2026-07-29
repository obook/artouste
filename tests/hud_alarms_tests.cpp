/*
 * hud_alarms_tests.cpp
 * Voyants d'alarme des cadrans (ui::hud_widgets::alarme*) : ce sont de simples
 * fonctions de l'état affiché, donc testables sans fenêtre ni contexte
 * graphique. On vérifie surtout le carburant, dont le voyant doit rester rouge
 * réservoir vide alors que le reste de la planche s'éteint.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "physics/constants.hpp"
#include "ui/HudAlarms.hpp"

#include <catch2/catch_test_macros.hpp>

using artouste::ui::HudData;
using artouste::ui::hud_widgets::alarmeCarb;
using artouste::ui::hud_widgets::GaugeLed;

namespace {

/* Planche sous tension : turbine au régime, réservoir plein. */
HudData enVol() {
    HudData d;
    d.turbineRpm = 33000.0f;
    d.fuelLiters = artouste::physics::FUEL_CAPACITY_L;
    return d;
}

} /* namespace */

TEST_CASE("Voyant carburant : les trois niveaux en vol", "[hud][alarmes]") {
    HudData d = enVol();
    CHECK(alarmeCarb(d) == GaugeLed::Green);

    d.fuelLiters = artouste::physics::FUEL_CAUTION_L - 1.0f;
    CHECK(alarmeCarb(d) == GaugeLed::Yellow);

    d.fuelLiters = artouste::physics::FUEL_LOW_L - 1.0f;
    CHECK(alarmeCarb(d) == GaugeLed::Red);
}

TEST_CASE("Voyant carburant : rouge réservoir vide, même turbine arrêtée", "[hud][alarmes]") {
    /* La planche s'éteint quand la turbine s'arrête, mais pas ce voyant-là : la
       panne sèche EST la raison de l'arrêt. L'éteindre effacerait l'explication au
       moment où le pilote la cherche. */
    HudData d = enVol();
    d.fuelLiters = 0.0f;
    d.turbineRpm = 0.0f;
    CHECK(alarmeCarb(d) == GaugeLed::Red);

    /* Turbine encore en train de descendre : rouge également. */
    d.turbineRpm = 12000.0f;
    CHECK(alarmeCarb(d) == GaugeLed::Red);
}

TEST_CASE("Voyant carburant : éteint au parking, réservoir plein", "[hud][alarmes]") {
    /* Avant le démarrage, la planche est hors tension et le voyant doit rester
       éteint : il n'y a rien à signaler. */
    HudData d = enVol();
    d.turbineRpm = 0.0f;
    CHECK(alarmeCarb(d) == GaugeLed::Off);
}
