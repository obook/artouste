/*
 * rotor_blur_tests.cpp
 * Vérifie le fondu entre les pales nettes et les plans flous du rotor
 * (render::heli_detail::blurFade). Pas de contexte graphique : c'est une
 * courbe pure.
 *
 * Auteur : O. Booklage
 * Date : septembre 2026
 * Licence : GPL v2
 */

#include "render/LoadedHelicopterDetail.hpp"

#include <catch2/catch_test_macros.hpp>

using artouste::render::heli_detail::blurFade;
using artouste::render::heli_detail::BLUR_FULL;
using artouste::render::heli_detail::BLUR_START;
using artouste::render::heli_detail::ROTOR_NOMINAL_RPM;

/* Régime rotor (tr/min) exprimé comme l'attend blurFade. */
static float fraction(float rpm) {
    return rpm / ROTOR_NOMINAL_RPM;
}

TEST_CASE("Rotor arrêté : pales nettes, aucun flou", "[rotor]") {
    CHECK(blurFade(0.0f) == 0.0f);
    CHECK(blurFade(BLUR_START) == 0.0f);
}

TEST_CASE("Aucun flou en dessous de 200 tr/min", "[rotor]") {
    CHECK(blurFade(fraction(0.0f)) == 0.0f);
    CHECK(blurFade(fraction(120.0f)) == 0.0f);
    CHECK(blurFade(fraction(199.0f)) == 0.0f);
    CHECK(blurFade(fraction(200.0f)) == 0.0f);
    /* Juste au-dessus, le flou démarre. */
    CHECK(blurFade(fraction(201.0f)) > 0.0f);
}

TEST_CASE("Régime de vol : flou seul", "[rotor]") {
    CHECK(blurFade(BLUR_FULL) == 1.0f);
    CHECK(blurFade(1.0f) == 1.0f);
}

TEST_CASE("Entre les deux seuils, le flou monte sans à-coup", "[rotor]") {
    const float milieu = 0.5f * (BLUR_START + BLUR_FULL);
    CHECK(blurFade(milieu) > 0.49f);
    CHECK(blurFade(milieu) < 0.51f);
    /* Croissante : jamais de retour en arrière du flou quand le régime monte. */
    float precedent = 0.0f;
    for (int i = 0; i <= 20; ++i) {
        const float f = blurFade(static_cast<float>(i) / 20.0f);
        CHECK(f >= precedent);
        precedent = f;
    }
}

TEST_CASE("Régime hors bornes : pas d'opacité négative ni supérieure à 1", "[rotor]") {
    CHECK(blurFade(-1.0f) == 0.0f);
    CHECK(blurFade(2.0f) == 1.0f);
}
