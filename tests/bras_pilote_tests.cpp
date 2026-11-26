/*
 * bras_pilote_tests.cpp
 * Tests de la cinématique inverse à deux os qui pose la main du pilote sur
 * une commande : épaule fixe, coude qui plie du même côté qu'au repos, main
 * sur la cible, os allongés si la cible est hors de portée.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#include "render/BrasPilote.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using artouste::mat3;
using artouste::mat4;
using artouste::vec3;
using artouste::vec4;
using Catch::Approx;
using namespace artouste::render;

namespace {

/* Bras gauche du pilote au repos, mesuré sur le .glb (repère racine). */
const OsBras BRAS{{-3.64f, -0.04f, -0.19f}, {-3.80f, -0.29f, -0.15f}, {-4.12f, -0.32f, -0.17f}};

vec3 appliquer(const mat4& m, const vec3& p) {
    return vec3(m * vec4(p, 1.0f));
}

void verifierIdentite(const mat4& m) {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            CHECK(m[c][r] == Approx(c == r ? 1.0f : 0.0f).margin(1e-5));
        }
    }
}

void verifierProche(const vec3& a, const vec3& b, float marge = 1e-4f) {
    CHECK(a.x == Approx(b.x).margin(marge));
    CHECK(a.y == Approx(b.y).margin(marge));
    CHECK(a.z == Approx(b.z).margin(marge));
}

}  /* namespace */

TEST_CASE("Cible sur la main au repos : le bras ne bouge pas", "[bras]") {
    const PoseBras p = poseBras(BRAS, BRAS.main);
    for (const vec3& q : {BRAS.epaule, BRAS.coude, BRAS.main, vec3{-3.9f, -0.2f, -0.1f}}) {
        verifierProche(appliquer(p.bras, q), q);
        verifierProche(appliquer(p.avantBras, q), q);
    }
}

TEST_CASE("Cible à portée : la main l'atteint, l'épaule reste, le coude tient", "[bras]") {
    /* À portée sans tendre le bras (moins de TENSION_MAX de sa longueur). */
    const vec3     cible = BRAS.main + vec3{0.02f, -0.06f, 0.06f};
    const PoseBras p     = poseBras(BRAS, cible);
    verifierProche(appliquer(p.avantBras, BRAS.main), cible);
    verifierProche(appliquer(p.bras, BRAS.epaule), BRAS.epaule);
    /* Le coude du bras et celui de l'avant-bras coïncident. */
    verifierProche(appliquer(p.bras, BRAS.coude), appliquer(p.avantBras, BRAS.coude));
    /* Les os gardent leur longueur. */
    CHECK(glm::length(appliquer(p.bras, BRAS.coude) - BRAS.epaule) ==
          Approx(glm::length(BRAS.coude - BRAS.epaule)).margin(1e-4));
}

TEST_CASE("Le coude plie du même côté qu'au repos", "[bras]") {
    const vec3     cible = BRAS.main + vec3{0.0f, -0.05f, 0.05f};
    const PoseBras p     = poseBras(BRAS, cible);
    /* Petit déplacement de la main : le coude bouge peu (pas de bascule). */
    CHECK(glm::length(appliquer(p.bras, BRAS.coude) - BRAS.coude) < 0.06f);
}

TEST_CASE("Cible hors de portée : la main l'atteint quand même", "[bras]") {
    const vec3     cible = BRAS.epaule + vec3{-0.70f, -0.30f, 0.05f};
    const PoseBras p     = poseBras(BRAS, cible);
    verifierProche(appliquer(p.avantBras, BRAS.main), cible, 1e-3f);
    verifierProche(appliquer(p.bras, BRAS.coude), appliquer(p.avantBras, BRAS.coude), 1e-3f);
    /* L'épaule avance au plus de EPAULE_AVANCE_MAX vers la cible. */
    CHECK(glm::length(appliquer(p.bras, BRAS.epaule) - BRAS.epaule) <=
          EPAULE_AVANCE_MAX + 1e-4f);
}

namespace {

/* Distance du coude posé à la ligne épaule-main posées : 0 si le bras est
   tendu. */
float pliDuCoude(const PoseBras& p) {
    const vec3 e = appliquer(p.bras, BRAS.epaule);
    const vec3 c = appliquer(p.bras, BRAS.coude);
    const vec3 m = appliquer(p.avantBras, BRAS.main);
    const vec3 u = glm::normalize(m - e);
    return glm::length((c - e) - u * glm::dot(c - e, u));
}

const float BRAS_TENDU = glm::length(BRAS.coude - BRAS.epaule) + glm::length(BRAS.main - BRAS.coude);

}  /* namespace */

TEST_CASE("Le bras ne se tend jamais : le coude reste plié", "[bras]") {
    const vec3 vers = glm::normalize(BRAS.main - BRAS.epaule);
    for (float f : {0.90f, 1.0f, 1.05f, 1.15f, 1.3f}) {
        const PoseBras p = poseBras(BRAS, BRAS.epaule + vers * (BRAS_TENDU * f));
        CHECK(pliDuCoude(p) > 0.05f);
    }
}

TEST_CASE("Un peu hors de portée : l'épaule avance, les os gardent leur longueur", "[bras]") {
    const vec3     vers  = glm::normalize(BRAS.main - BRAS.epaule);
    const vec3     cible = BRAS.epaule + vers * (BRAS_TENDU * 1.03f);
    const PoseBras p     = poseBras(BRAS, cible);
    const vec3     e     = appliquer(p.bras, BRAS.epaule);
    CHECK(glm::length(e - BRAS.epaule) > 0.0f);
    CHECK(glm::length(appliquer(p.bras, BRAS.coude) - e) ==
          Approx(glm::length(BRAS.coude - BRAS.epaule)).margin(1e-4));
    verifierProche(appliquer(p.avantBras, BRAS.main), cible, 1e-3f);
}

TEST_CASE("Jambe presque tendue : la pose de repos ne bouge pas, la hanche reste", "[bras]") {
    /* Jambe gauche du pilote (hanche, genou, avant du pied), tendue à 95 %. */
    const OsBras jambe{{-3.76f, -0.52f, -0.25f}, {-4.12f, -0.47f, -0.25f}, {-4.55f, -0.70f, -0.20f}};
    const PoseBras repos = poseBras(jambe, jambe.main, 0.99f, 0.0f);
    for (const vec3& q : {jambe.epaule, jambe.coude, jambe.main}) {
        verifierProche(appliquer(repos.bras, q), q);
        verifierProche(appliquer(repos.avantBras, q), q);
    }
    /* Pédale poussée de 4 cm : le pied suit, la hanche ne bouge pas. */
    const vec3     cible = jambe.main + vec3{-0.04f, 0.0f, -0.08f};
    const PoseBras p     = poseBras(jambe, cible, 0.99f, 0.0f);
    verifierProche(appliquer(p.avantBras, jambe.main), cible, 1e-3f);
    verifierProche(appliquer(p.bras, jambe.epaule), jambe.epaule);
}

TEST_CASE("Symétrie de l'avant-bras : coude et main restent, le pouce change de côté", "[bras]") {
    const mat4 s = symetrieAvantBras(BRAS);
    verifierProche(appliquer(s, BRAS.coude), BRAS.coude);
    verifierProche(appliquer(s, BRAS.main), BRAS.main);
    /* Un point sur le côté de la main passe de l'autre côté, à même distance. */
    const vec3 axe    = glm::normalize(BRAS.main - BRAS.coude);
    const vec3 cote   = glm::normalize(glm::cross(axe, vec3{0.0f, 1.0f, 0.0f}));
    const vec3 pouce  = BRAS.main + cote * 0.04f;
    verifierProche(appliquer(s, pouce), BRAS.main - cote * 0.04f);
    /* Un point au-dessus de la main ne bouge pas. */
    const vec3 dessus = BRAS.main + vec3{0.0f, 0.03f, 0.0f} - axe * glm::dot(axe, vec3{0.0f, 0.03f, 0.0f});
    verifierProche(appliquer(s, dessus), dessus);
    CHECK(glm::determinant(mat3(s)) == Approx(-1.0f).margin(1e-5));
}

TEST_CASE("Rotation de la main sur l'axe de l'avant-bras", "[bras]") {
    /* Vu de derrière la main (depuis le coude), 90 deg dans le sens inverse
       des aiguilles d'une montre : le côté droit de la main passe en haut. */
    const mat4 r      = rotationAvantBras(BRAS, 90.0f);
    const vec3 axe    = glm::normalize(BRAS.main - BRAS.coude);
    const vec3 droite = glm::normalize(glm::cross(axe, vec3{0.0f, 1.0f, 0.0f}));
    const vec3 haut   = glm::cross(droite, axe);
    verifierProche(appliquer(r, BRAS.coude), BRAS.coude);
    verifierProche(appliquer(r, BRAS.main), BRAS.main);
    verifierProche(appliquer(r, BRAS.main + droite * 0.04f), BRAS.main + haut * 0.04f);
    verifierIdentite(rotationAvantBras(BRAS, 0.0f));
}
