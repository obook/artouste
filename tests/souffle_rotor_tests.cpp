/*
 * souffle_rotor_tests.cpp
 * Vérifie la simulation du souffle rotor (app::SouffleRotor) : la courbe
 * d'intensité selon la hauteur sol et le régime, puis le comportement du nuage
 * de bouffées (émission, écartement, extinction, plafond de coût). Aucun
 * contexte graphique n'est nécessaire : la classe ne tient que la simulation.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/SouffleRotor.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>

using artouste::vec3;
using artouste::app::SouffleRotor;

namespace {

/* Sol plat à l'altitude zéro : le cas de référence des essais. */
float solPlat(float, float) {
    return 0.0f;
}

/* Fait tourner la simulation pendant 'duree' secondes, par pas de 1/60 s, avec
   l'appareil immobile à la hauteur donnée. */
void avancer(SouffleRotor& souffle, float duree, float hauteur, float rotor, float collectif) {
    constexpr float DT = 1.0f / 60.0f;
    for (float t = 0.0f; t < duree; t += DT) {
        souffle.update(DT, vec3{0.0f, hauteur, 0.0f}, rotor, collectif, solPlat);
    }
}

/* Distance horizontale moyenne des bouffées à l'axe du mât. */
float rayonMoyen(const SouffleRotor& souffle) {
    float somme = 0.0f;
    for (const SouffleRotor::Bouffee& b : souffle.bouffees()) {
        somme += std::sqrt(b.centre.x * b.centre.x + b.centre.z * b.centre.z);
    }
    const std::size_t n = souffle.bouffees().size();
    return n == 0 ? 0.0f : somme / static_cast<float>(n);
}

} /* namespace */

TEST_CASE("intensité : rien sans rotor", "[souffle]") {
    const SouffleRotor souffle;
    REQUIRE(souffle.intensite(0.0f, 0.0f, 1.0f) == 0.0f);
}

TEST_CASE("intensité : maximale posé, rotor au régime", "[souffle]") {
    const SouffleRotor souffle;
    REQUIRE(souffle.intensite(0.0f, 1.0f, 1.0f) > 0.9f);
}

TEST_CASE("intensité : nulle au-dessus du plafond", "[souffle]") {
    const SouffleRotor souffle;
    /* Le plafond vaut plusieurs rayons rotor : au-delà, le flux descendant s'est
       dispersé avant d'atteindre le sol. */
    REQUIRE(souffle.plafondM() > 5.0f);
    REQUIRE(souffle.intensite(souffle.plafondM() + 1.0f, 1.0f, 1.0f) == 0.0f);
}

TEST_CASE("intensité : décroît dès les premiers mètres, sans palier", "[souffle]") {
    const SouffleRotor souffle;
    /* Le souffle s'étale en descendant : chaque mètre gagné en compte. Un palier
       sous le rayon rotor rendrait toute la finale identique, ce qui est
       justement ce qu'on ne veut pas. */
    const float contact = souffle.intensite(0.0f, 1.0f, 1.0f);
    const float unMetre = souffle.intensite(1.0f, 1.0f, 1.0f);
    const float quatreMetres = souffle.intensite(4.0f, 1.0f, 1.0f);
    REQUIRE(unMetre < contact);
    REQUIRE(quatreMetres < unMetre * 0.85f);
}

TEST_CASE("intensité : il en reste un peu en stationnaire haut", "[souffle]") {
    const SouffleRotor souffle;
    /* À cinq et dix mètres, l'appareil soulève encore de la poussière : bien
       moins qu'au contact, mais assez pour se voir. Une décroissance trop
       brutale éteignait tout dès le milieu de la finale. */
    const float contact = souffle.intensite(0.0f, 1.0f, 1.0f);
    const float cinq = souffle.intensite(5.0f, 1.0f, 1.0f);
    const float dix = souffle.intensite(10.0f, 1.0f, 1.0f);
    REQUIRE(cinq > 0.40f * contact);
    REQUIRE(dix > 0.15f * contact);
    REQUIRE(dix < cinq);
}

TEST_CASE("intensité : décroît avec la hauteur", "[souffle]") {
    const SouffleRotor souffle;
    const float bas = souffle.intensite(2.0f, 1.0f, 1.0f);
    const float milieu = souffle.intensite(8.0f, 1.0f, 1.0f);
    const float haut = souffle.intensite(11.0f, 1.0f, 1.0f);
    REQUIRE(bas > milieu);
    REQUIRE(milieu > haut);
    REQUIRE(haut >= 0.0f);
}

TEST_CASE("intensité : décroît avec le régime rotor", "[souffle]") {
    const SouffleRotor souffle;
    const float plein = souffle.intensite(0.0f, 1.0f, 1.0f);
    const float moitie = souffle.intensite(0.0f, 0.5f, 1.0f);
    /* Poussée en carré du régime : à mi-régime, il reste le quart du souffle. */
    REQUIRE(moitie < plein * 0.30f);
    REQUIRE(moitie > 0.0f);
}

TEST_CASE("intensité : le collectif module sans jamais tout couper", "[souffle]") {
    const SouffleRotor souffle;
    const float manchesEnBas = souffle.intensite(0.0f, 1.0f, 0.0f);
    const float pleinPas = souffle.intensite(0.0f, 1.0f, 1.0f);
    REQUIRE(manchesEnBas > 0.0f); /* rotor au régime : l'appareil brasse déjà de l'air */
    REQUIRE(pleinPas > manchesEnBas);
}

TEST_CASE("nuage : rien ne se lève rotor arrêté", "[souffle]") {
    SouffleRotor souffle;
    avancer(souffle, 2.0f, 0.5f, 0.0f, 0.5f);
    REQUIRE(souffle.bouffees().empty());
}

TEST_CASE("nuage : rien ne se lève en altitude", "[souffle]") {
    SouffleRotor souffle;
    avancer(souffle, 2.0f, 200.0f, 1.0f, 1.0f);
    REQUIRE(souffle.bouffees().empty());
}

TEST_CASE("nuage : la poussière se lève près du sol", "[souffle]") {
    SouffleRotor souffle;
    avancer(souffle, 1.0f, 1.0f, 1.0f, 0.8f);
    REQUIRE(!souffle.bouffees().empty());
    for (const SouffleRotor::Bouffee& b : souffle.bouffees()) {
        REQUIRE(b.opacite >= 0.0f);
        REQUIRE(b.diametre > 0.0f);
        REQUIRE(b.centre.y >= 0.0f); /* jamais sous le terrain */
    }
}

TEST_CASE("nuage : les bouffées s'écartent puis remontent", "[souffle]") {
    SouffleRotor souffle;
    avancer(souffle, 0.5f, 1.0f, 1.0f, 0.8f);
    const float rayonTot = rayonMoyen(souffle);
    const float hauteurTot = souffle.bouffees().front().centre.y;
    avancer(souffle, 1.5f, 1.0f, 1.0f, 0.8f);
    REQUIRE(rayonMoyen(souffle) > rayonTot);
    /* Recirculation : le nuage ne reste pas plaqué au sol, il monte. */
    float hauteurMax = 0.0f;
    for (const SouffleRotor::Bouffee& b : souffle.bouffees()) {
        hauteurMax = b.centre.y > hauteurMax ? b.centre.y : hauteurMax;
    }
    REQUIRE(hauteurMax > hauteurTot);
}

TEST_CASE("nuage : il s'éteint quand le rotor s'arrête", "[souffle]") {
    SouffleRotor souffle;
    avancer(souffle, 2.0f, 1.0f, 1.0f, 0.8f);
    REQUIRE(!souffle.bouffees().empty());
    /* Plus longtemps que la vie d'une bouffée : tout doit être retombé. */
    avancer(souffle, 4.0f, 1.0f, 0.0f, 0.0f);
    REQUIRE(souffle.bouffees().empty());
}

TEST_CASE("nuage : le nombre de bouffées reste sous la capacité", "[souffle]") {
    constexpr std::size_t CAPACITE = 50;
    SouffleRotor souffle(5.0f, CAPACITE);
    avancer(souffle, 10.0f, 0.5f, 1.0f, 1.0f);
    REQUIRE(souffle.bouffees().size() <= CAPACITE);
}

TEST_CASE("nuage : un pas nul fige le nuage sans l'effacer", "[souffle]") {
    SouffleRotor souffle;
    avancer(souffle, 1.0f, 1.0f, 1.0f, 0.8f);
    const std::size_t avant = souffle.bouffees().size();
    const vec3 premiere = souffle.bouffees().front().centre;
    souffle.update(0.0f, vec3{0.0f, 1.0f, 0.0f}, 1.0f, 0.8f, solPlat);
    REQUIRE(souffle.bouffees().size() == avant);
    REQUIRE(souffle.bouffees().front().centre == premiere);
}

TEST_CASE("nuage : vider efface tout", "[souffle]") {
    SouffleRotor souffle;
    avancer(souffle, 1.0f, 1.0f, 1.0f, 0.8f);
    REQUIRE(!souffle.bouffees().empty());
    souffle.vider();
    REQUIRE(souffle.bouffees().empty());
}
