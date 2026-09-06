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

/*
 * Le régime nominal de la turbine (34 000 tr/min) est le HAUT de la bande verte
 * du cadran, et le régulateur fait respirer le régime autour de ce nominal.
 * Sans marge, la LED virait au jaune à chaque inspiration.
 */
TEST_CASE("Voyant turbine : le battement du régulateur ne le fait pas clignoter",
          "[hud][alarmes][turbine]") {
    using artouste::ui::hud_widgets::alarmeTurbine;
    using artouste::ui::hud_widgets::TURBINE_RPM_NOMINAL;
    using artouste::ui::hud_widgets::TURBINE_RPM_TOLERANCE;

    HudData d;

    SECTION("vert sur tout le battement, en haut comme en bas") {
        const float amplitude = TURBINE_RPM_NOMINAL * artouste::physics::BATTEMENT_REGIME;
        for (int i = -20; i <= 20; ++i) {
            d.turbineRpm = TURBINE_RPM_NOMINAL + amplitude * static_cast<float>(i) / 20.0f;
            CHECK(alarmeTurbine(d) == GaugeLed::Green);
        }
        /* La marge couvre le battement avec de la réserve. */
        CHECK(TURBINE_RPM_TOLERANCE > amplitude);
    }

    SECTION("un vrai surrégime allume toujours le jaune") {
        d.turbineRpm = TURBINE_RPM_NOMINAL + TURBINE_RPM_TOLERANCE + 1.0f;
        CHECK(alarmeTurbine(d) == GaugeLed::Yellow);
    }

    SECTION("et le rouge plus haut encore") {
        d.turbineRpm = 34500.0f + TURBINE_RPM_TOLERANCE + 1.0f;
        CHECK(alarmeTurbine(d) == GaugeLed::Red);
    }

    SECTION("la bande verte garde son plancher") {
        d.turbineRpm = 33000.0f;
        CHECK(alarmeTurbine(d) == GaugeLed::Green);
        d.turbineRpm          = 32999.0f;
        d.turbineSpoolingUp   = false;
        CHECK(alarmeTurbine(d) == GaugeLed::Off);
    }
}

/*
 * Sous-régime turbine établie : le régulateur n'a pas suivi la charge (droop
 * transitoire). Le voyant doit alerter, pas s'éteindre.
 */
TEST_CASE("Voyant turbine : le creux de droop alerte au lieu d'éteindre",
          "[hud][alarmes][turbine]") {
    using artouste::ui::hud_widgets::alarmeTurbine;

    HudData d;
    d.turbineRpm = 32300.0f;  /* creux d'une action franche au collectif */

    SECTION("turbine établie : jaune") {
        d.turbineEtabli = true;
        CHECK(alarmeTurbine(d) == GaugeLed::Yellow);
    }

    SECTION("pendant le démarrage : clignotement, pas d'alerte") {
        d.turbineEtabli     = false;
        d.turbineSpoolingUp = true;
        d.alarmBlinkOn      = true;
        CHECK(alarmeTurbine(d) == GaugeLed::Green);
    }

    SECTION("pendant l'extinction : éteint, un régime bas y est normal") {
        d.turbineEtabli     = false;
        d.turbineSpoolingUp = false;
        CHECK(alarmeTurbine(d) == GaugeLed::Off);
    }
}
