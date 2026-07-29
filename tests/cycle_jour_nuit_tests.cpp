/*
 * cycle_jour_nuit_tests.cpp
 * Vérifie l'heure simulée (app::heureDuJour) : nuit plus rapide que le jour,
 * continuité au coucher et au lever, durée d'un cycle complet, et cas
 * particuliers (temps figé, marche arrière, facteur absurde).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "app/CycleJourNuit.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using artouste::app::heureDuJour;
using Catch::Approx;

namespace {

constexpr float MIDI = 12.0f * 3600.0f;
constexpr float LEVER = 6.0f * 3600.0f;
constexpr float COUCHER = 18.0f * 3600.0f;
constexpr float MINUIT = 0.0f;

} /* namespace */

TEST_CASE("la nuit passe deux fois plus vite que le jour", "[cycle]") {
    /* Réglages par défaut : jour accéléré 72 fois, nuit deux fois plus vite
       encore. Départ à midi. Le soleil met donc 6 h simulées (21600 s) à se
       coucher, soit 300 s réelles, puis 12 h de nuit à 144 fois, soit 300 s
       également : la nuit entière dure aussi longtemps qu'une demi-journée. */
    const float vJour = 72.0f;
    const float fNuit = 2.0f;

    REQUIRE(heureDuJour(MIDI, 0.0f, vJour, fNuit) == Approx(MIDI));
    REQUIRE(heureDuJour(MIDI, 300.0f, vJour, fNuit) == Approx(COUCHER));             /* coucher */
    REQUIRE(heureDuJour(MIDI, 450.0f, vJour, fNuit) == Approx(MINUIT).margin(0.5f)); /* minuit */
    REQUIRE(heureDuJour(MIDI, 600.0f, vJour, fNuit) == Approx(LEVER));               /* lever */
    /* Un cycle complet (10 min de jour + 5 min de nuit) ramène à l'heure de
       départ, à la seconde près. */
    REQUIRE(heureDuJour(MIDI, 900.0f, vJour, fNuit) == Approx(MIDI).margin(0.5f));
}

TEST_CASE("facteur 1 : le temps avance uniformément, comme avant", "[cycle]") {
    const float vJour = 72.0f;
    for (const float t : {0.0f, 100.0f, 617.0f, 1234.0f}) {
        const float attendu = std::fmod(MIDI + t * vJour, 86400.0f);
        INFO("t = " << t);
        REQUIRE(heureDuJour(MIDI, t, vJour, 1.0f) == Approx(attendu).margin(0.5f));
    }
}

TEST_CASE("l'heure ne saute ni au coucher ni au lever", "[cycle]") {
    /* Le changement de vitesse doit être invisible : deux instants voisins de part
       et d'autre du coucher donnent deux heures voisines. Sans continuité, le ciel
       ferait un bond au crépuscule. */
    const float vJour = 72.0f;
    const float fNuit = 2.0f;
    for (const float bascule : {300.0f, 600.0f}) { /* coucher, puis lever */
        const float avant = heureDuJour(MIDI, bascule - 0.05f, vJour, fNuit);
        const float apres = heureDuJour(MIDI, bascule + 0.05f, vJour, fNuit);
        float ecart = std::fabs(apres - avant);
        if (ecart > 43200.0f) {
            ecart = 86400.0f - ecart; /* passage par minuit */
        }
        INFO("bascule à t = " << bascule);
        REQUIRE(ecart < 20.0f); /* moins de 20 s simulées pour 0,1 s réelle */
    }
}

TEST_CASE("départ de nuit : la vitesse de nuit s'applique tout de suite", "[cycle]") {
    /* Minuit, 6 h simulées avant le lever. À 144 fois le temps réel, il reste
       150 s réelles à voler dans le noir. */
    const float vJour = 72.0f;
    const float fNuit = 2.0f;
    REQUIRE(heureDuJour(MINUIT, 0.0f, vJour, fNuit) == Approx(MINUIT).margin(0.5f));
    REQUIRE(heureDuJour(MINUIT, 150.0f, vJour, fNuit) == Approx(LEVER).margin(0.5f));
    /* Puis le jour reprend sa vitesse : 300 s de plus donnent midi. */
    REQUIRE(heureDuJour(MINUIT, 450.0f, vJour, fNuit) == Approx(MIDI).margin(0.5f));
}

TEST_CASE("temps figé : l'heure ne bouge pas", "[cycle]") {
    REQUIRE(heureDuJour(MIDI, 0.0f, 0.0f, 2.0f) == Approx(MIDI));
    REQUIRE(heureDuJour(MIDI, 10000.0f, 0.0f, 2.0f) == Approx(MIDI));
}

TEST_CASE("marche arrière : rythme uniforme et heure valide", "[cycle]") {
    /* Une échelle négative fait tourner le cycle à l'envers ; le découpage en deux
       vitesses n'aurait pas de sens, mais l'heure doit rester dans la journée. */
    const float h = heureDuJour(MIDI, 100.0f, -72.0f, 2.0f);
    REQUIRE(h >= 0.0f);
    REQUIRE(h < 86400.0f);
    REQUIRE(h == Approx(MIDI - 7200.0f).margin(0.5f));
}

TEST_CASE("facteur absurde : la nuit finit quand même par passer", "[cycle]") {
    /* Un facteur nul ou négatif arrêterait la nuit pour toujours. Il est borné,
       donc l'heure avance encore, seulement très lentement. */
    for (const float facteur : {0.0f, -5.0f}) {
        INFO("facteur " << facteur);
        const float depart = heureDuJour(MINUIT, 0.0f, 72.0f, facteur);
        const float apres = heureDuJour(MINUIT, 500.0f, 72.0f, facteur);
        REQUIRE(apres != Approx(depart));
        REQUIRE(apres >= 0.0f);
        REQUIRE(apres < 86400.0f);
    }
}

TEST_CASE("l'heure reste toujours dans la journée", "[cycle]") {
    for (const float t : {-10000.0f, -1.0f, 0.0f, 37.0f, 5000.0f, 123456.0f}) {
        const float h = heureDuJour(MIDI, t, 72.0f, 2.0f);
        INFO("t = " << t << " -> " << h);
        REQUIRE(h >= 0.0f);
        REQUIRE(h < 86400.0f);
    }
}
