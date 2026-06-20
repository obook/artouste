/*
 * flight_assist_tests.cpp
 * Tests du mode assisté (FlightAssist). Logique pure, sans contexte graphique :
 * on vérifie que l'assistance laisse passer les commandes au repos, qu'elle borne
 * la variation du collectif, lisse les inputs, ramène le cyclique au neutre, et
 * que la bascule entre les deux modes est progressive.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "physics/FlightAssist.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cmath>

using artouste::physics::Controls;
using artouste::physics::FlightAssist;

namespace {

/* Applique la même commande pendant `seconds`, par petits pas fixes, et renvoie
 * la dernière sortie de l'assistance. */
Controls run(FlightAssist& assist, const Controls& raw, float seconds) {
    constexpr float step = 1.0f / 60.0f;
    Controls        out;
    for (float t = 0.0f; t < seconds; t += step) {
        out = assist.apply(raw, step);
    }
    return out;
}

}  /* namespace */

TEST_CASE("Assistance inactive : les commandes passent telles quelles", "[assist]") {
    FlightAssist assist;
    Controls     raw;
    raw.cyclicLateral      = 0.7f;
    raw.cyclicLongitudinal = -0.4f;
    raw.collective         = 0.8f;
    raw.pedals             = 0.3f;

    const Controls out = assist.apply(raw, 1.0f / 60.0f);

    REQUIRE(out.cyclicLateral == raw.cyclicLateral);
    REQUIRE(out.cyclicLongitudinal == raw.cyclicLongitudinal);
    REQUIRE(out.collective == raw.collective);
    REQUIRE(out.pedals == raw.pedals);
    REQUIRE_FALSE(assist.active());
}

TEST_CASE("La bascule du mode assisté est progressive", "[assist]") {
    FlightAssist assist;
    assist.toggle();
    REQUIRE(assist.active());

    /* Juste après la bascule, l'intensité n'est pas encore pleine. */
    assist.apply(Controls{}, 1.0f / 60.0f);
    REQUIRE(assist.intensity() > 0.0f);
    REQUIRE(assist.intensity() < 1.0f);

    /* Au bout d'une seconde, l'assistance est pleinement établie. */
    run(assist, Controls{}, 1.0f);
    REQUIRE(assist.intensity() > 0.99f);
}

TEST_CASE("Le collectif ne varie pas plus vite que la limite", "[assist]") {
    FlightAssist assist;
    assist.toggle();
    run(assist, Controls{}, 1.0f);  /* assistance pleinement établie, collectif a 0 */

    Controls raw;
    raw.collective = 1.0f;  /* coup de collectif plein, d'un coup */

    /* Sur une seule image, la consigne ne doit presque pas bouger. */
    const Controls out = assist.apply(raw, 1.0f / 60.0f);
    REQUIRE(out.collective < 0.1f);

    /* Il faut environ deux secondes (taux 0,5/s) pour atteindre le plein collectif. */
    const Controls settled = run(assist, raw, 3.0f);
    REQUIRE(settled.collective > 0.95f);
}

TEST_CASE("Le cyclique revient au neutre en l'absence d'input", "[assist]") {
    FlightAssist assist;
    assist.toggle();

    /* On tient le manche poussé, l'assistance suit la consigne. */
    Controls pushed;
    pushed.cyclicLongitudinal = 0.9f;
    const Controls held = run(assist, pushed, 2.0f);
    REQUIRE(held.cyclicLongitudinal > 0.5f);

    /* On relâche : la consigne brute repasse a zéro, le manche est rappelé au neutre. */
    const Controls released = run(assist, Controls{}, 4.0f);
    REQUIRE(std::fabs(released.cyclicLongitudinal) < 0.05f);
}

TEST_CASE("Un input brusque sur le cyclique est lissé", "[assist]") {
    FlightAssist assist;
    assist.toggle();
    run(assist, Controls{}, 1.0f);  /* assistance établie, manche au neutre */

    Controls raw;
    raw.cyclicLateral = 1.0f;  /* a fond d'un coup */

    /* En une image, la sortie n'a pas encore rejoint la consigne (lissage). */
    const Controls out = assist.apply(raw, 1.0f / 60.0f);
    REQUIRE(out.cyclicLateral < 0.9f);
}
