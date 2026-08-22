/*
 * tablier_pont_tests.cpp
 * Extrusion d'un ouvrage d'art (render/buildings/BuildingsGeometrie.hpp) : le
 * ruban posé à l'altitude de la chaussée que la BD TOPO relève, là où le modèle
 * de terrain ne connaît que le sol nu. Du calcul pur, sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "render/buildings/BuildingsGeometrie.hpp"

using namespace artouste;
using namespace artouste::render;
using Catch::Matchers::WithinAbs;

namespace {

const vec3 GRIS{0.5f, 0.5f, 0.5f};

/* Emprise fictive de 1000 x 1000 m dont le coin nord-ouest est à l'origine. */
const CalageOrtho ORTHO{0.0f, 0.0f, 1000.0f, 1000.0f};

/* Les deux flots de l'extrusion, réunis pour les essais qui portent sur la
   géométrie et non sur le shader qui la dessine. */
struct Tablier {
    std::vector<Vertex>       beton, chaussee;
    std::vector<unsigned int> idxBeton, idxChaussee;

    void extruder(const std::vector<float>& px, const std::vector<float>& py,
                  const std::vector<float>& pz, float largeur) {
        extruderTablier(beton, idxBeton, chaussee, idxChaussee, px, py, pz, largeur, GRIS, ORTHO);
    }
    [[nodiscard]] std::vector<Vertex> tous() const {
        std::vector<Vertex> v = beton;
        v.insert(v.end(), chaussee.begin(), chaussee.end());
        return v;
    }
};

/* Bornes du nuage de sommets produit, sur les trois axes. */
struct Boite {
    float minX, maxX, minY, maxY, minZ, maxZ;
};

[[nodiscard]] Boite boiteDe(const std::vector<Vertex>& v) {
    Boite b{v[0].position.x, v[0].position.x, v[0].position.y,
            v[0].position.y, v[0].position.z, v[0].position.z};
    for (const Vertex& s : v) {
        b.minX = std::min(b.minX, s.position.x);
        b.maxX = std::max(b.maxX, s.position.x);
        b.minY = std::min(b.minY, s.position.y);
        b.maxY = std::max(b.maxY, s.position.y);
        b.minZ = std::min(b.minZ, s.position.z);
        b.maxZ = std::max(b.maxZ, s.position.z);
    }
    return b;
}

} /* namespace */

TEST_CASE("un tablier droit occupe sa largeur à l'altitude de la chaussée") {
    /* Axe de 100 m le long de X, chaussée à 150 m, largeur 12 m. */
    const std::vector<float>  px{0.0f, 100.0f};
    const std::vector<float>  py{150.0f, 150.0f};
    const std::vector<float>  pz{0.0f, 0.0f};
    Tablier t;

    t.extruder(px, py, pz, 12.0f);

    /* Cinq quads de béton (dessous, deux flancs, deux capots) et un de
       chaussée, ce dernier parti dans la passe du terrain. */
    REQUIRE(t.beton.size() == 20);
    REQUIRE(t.idxBeton.size() == 30);
    REQUIRE(t.chaussee.size() == 4);
    REQUIRE(t.idxChaussee.size() == 6);

    const Boite b = boiteDe(t.tous());
    CHECK_THAT(b.minX, WithinAbs(0.0f, 1e-3f));
    CHECK_THAT(b.maxX, WithinAbs(100.0f, 1e-3f));
    /* Le ruban s'étend de part et d'autre de l'axe, pas d'un seul côté. */
    CHECK_THAT(b.minZ, WithinAbs(-6.0f, 1e-3f));
    CHECK_THAT(b.maxZ, WithinAbs(6.0f, 1e-3f));
    /* La chaussée est le HAUT du tablier ; l'épaisseur pend en dessous. */
    CHECK_THAT(b.maxY, WithinAbs(150.0f, 1e-3f));
    CHECK_THAT(b.minY, WithinAbs(150.0f - EPAISSEUR_TABLIER_M, 1e-3f));
}

TEST_CASE("le tablier suit la pente du profil altimétrique") {
    /* Une rampe d'accès : 140 m au départ, 150 m à l'arrivée. */
    const std::vector<float>  px{0.0f, 50.0f, 100.0f};
    const std::vector<float>  py{140.0f, 145.0f, 150.0f};
    const std::vector<float>  pz{0.0f, 0.0f, 0.0f};
    Tablier t;

    t.extruder(px, py, pz, 8.0f);

    const Boite b = boiteDe(t.tous());
    CHECK_THAT(b.maxY, WithinAbs(150.0f, 1e-3f));
    CHECK_THAT(b.minY, WithinAbs(140.0f - EPAISSEUR_TABLIER_M, 1e-3f));
}

TEST_CASE("aucune face du tablier ne passe pour un mur de façade") {
    /* Le shader trie mur et toit sur la seule normale : |n.y| >= 0,5 donne la
       couleur du sommet, en dessous il plaque la texture de façade et ses
       fenêtres. Un tablier ne doit jamais tomber du mauvais côté. */
    const std::vector<float>  px{0.0f, 60.0f, 120.0f};
    const std::vector<float>  py{150.0f, 151.0f, 150.0f};
    const std::vector<float>  pz{0.0f, 40.0f, 90.0f};
    Tablier t;

    t.extruder(px, py, pz, 10.0f);

    REQUIRE_FALSE(t.beton.empty());
    for (const Vertex& v : t.beton) {
        CHECK(std::fabs(v.normal.y) >= 0.5f);
    }
}

TEST_CASE("les deux bouts du tablier sont fermés") {
    /* La BD TOPO coupe le réseau à chaque intersection : un pont arrive en
       plusieurs tronçons, et une tranche ouverte se verrait de côté. */
    const std::vector<float>  px{0.0f, 40.0f, 80.0f};
    const std::vector<float>  py{150.0f, 150.0f, 150.0f};
    const std::vector<float>  pz{0.0f, 0.0f, 0.0f};
    Tablier t;

    t.extruder(px, py, pz, 10.0f);

    /* Deux segments : 2 x 3 faces de béton, plus un capot à chaque bout. */
    CHECK(t.beton.size() == 4 * (2 * 3 + 2));
    CHECK(t.chaussee.size() == 4 * 2);

    /* Un capot regarde le long de l'axe, pas en travers : ici l'axe est X, on
       attend donc deux faces dont la normale a une composante X non nulle. */
    std::size_t capots = 0;
    for (const Vertex& v : t.beton) {
        if (std::fabs(v.normal.x) > 0.1f) {
            ++capots;
        }
    }
    CHECK(capots == 8); /* deux quads de quatre sommets */
}

TEST_CASE("un axe d'un seul point ne produit rien") {
    const std::vector<float>  px{10.0f};
    const std::vector<float>  py{150.0f};
    const std::vector<float>  pz{10.0f};
    Tablier t;

    t.extruder(px, py, pz, 10.0f);

    CHECK(t.beton.empty());
    CHECK(t.chaussee.empty());
}

TEST_CASE("deux points confondus ne font pas diverger la perpendiculaire") {
    /* Doublon dans l'axe : la direction du segment est nulle, et une division
       par sa longueur donnerait des coordonnées non finies. */
    const std::vector<float>  px{0.0f, 0.0f, 50.0f};
    const std::vector<float>  py{150.0f, 150.0f, 150.0f};
    const std::vector<float>  pz{0.0f, 0.0f, 0.0f};
    Tablier t;

    t.extruder(px, py, pz, 10.0f);

    REQUIRE_FALSE(t.beton.empty());
    for (const Vertex& v : t.tous()) {
        CHECK(std::isfinite(v.position.x));
        CHECK(std::isfinite(v.position.y));
        CHECK(std::isfinite(v.position.z));
        CHECK(std::isfinite(v.normal.y));
    }
}

TEST_CASE("la chaussée porte le drapage de l'orthophoto") {
    /* C'est ce drapage qui fait montrer au tablier la photo du pont plutôt qu'un
       aplat gris : l'UV doit suivre la MÊME règle que le maillage du terrain,
       U vers l'est et V inversée, la rangée 0 de la carte étant au nord. */
    const std::vector<float> px{250.0f, 750.0f};
    const std::vector<float> py{150.0f, 150.0f};
    const std::vector<float> pz{400.0f, 400.0f};
    Tablier                  t;

    t.extruder(px, py, pz, 20.0f);

    REQUIRE(t.chaussee.size() == 4);
    for (const Vertex& v : t.chaussee) {
        CHECK_THAT(v.uv.x, WithinAbs(v.position.x / 1000.0f, 1e-4f));
        CHECK_THAT(v.uv.y, WithinAbs(1.0f - v.position.z / 1000.0f, 1e-4f));
    }
    /* Le béton, lui, n'est pas drapé : il garde la couleur unie. */
    for (const Vertex& v : t.beton) {
        CHECK_THAT(v.uv.x, WithinAbs(0.0f, 1e-6f));
        CHECK_THAT(v.uv.y, WithinAbs(0.0f, 1e-6f));
    }
}
