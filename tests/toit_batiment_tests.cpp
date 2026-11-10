/*
 * toit_batiment_tests.cpp
 * Toit d'un bâtiment (render/buildings/BuildingsGeometrie.hpp) : deux pans sur
 * une emprise de maison, toit plat juste sur une emprise concave. Du calcul
 * pur, sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

#include "render/buildings/BuildingsGeometrie.hpp"

using namespace artouste;
using namespace artouste::render;
using Catch::Matchers::WithinAbs;

namespace {

const vec3 GRIS{0.5f, 0.5f, 0.5f};

struct Batiment {
    std::vector<Vertex>       v;
    std::vector<unsigned int> i;
    float                     sommet = 0.0f;

    /* Base à 100 m, gouttière à 6 m au-dessus par défaut. */
    Batiment(const std::vector<float>& px, const std::vector<float>& pz, float toit,
             bool ardoise = false, float hauteur = 6.0f) {
        sommet = extruderBatiment(v, i, px, pz, 100.0f, hauteur, toit, ardoise, GRIS, GRIS);
    }

    /* Plus haut point des murs (normale horizontale). */
    [[nodiscard]] float hautDesMurs() const {
        float h = 0.0f;
        for (const Vertex& s : v) {
            if (std::fabs(s.normal.y) < 0.01f) {
                h = std::max(h, s.position.y);
            }
        }
        return h;
    }

    /* Surface en plan des triangles de toit plat (normale verticale). */
    [[nodiscard]] float surfaceToitPlat() const {
        float s = 0.0f;
        for (std::size_t k = 0; k + 2 < i.size(); k += 3) {
            const Vertex& a = v[i[k]];
            const Vertex& b = v[i[k + 1]];
            const Vertex& c = v[i[k + 2]];
            if (a.normal.y > 0.99f) {
                s += 0.5f * std::fabs((b.position.x - a.position.x) * (c.position.z - a.position.z) -
                                      (c.position.x - a.position.x) * (b.position.z - a.position.z));
            }
        }
        return s;
    }
};

/* Maison de 10 m (est-ouest) sur 6 m. */
const std::vector<float> MAISON_X{0.0f, 10.0f, 10.0f, 0.0f};
const std::vector<float> MAISON_Z{0.0f, 0.0f, 6.0f, 6.0f};

} /* namespace */

TEST_CASE("Toit à deux pans : faîtage sur le grand axe, à la hauteur BD TOPO", "[toit]") {
    const Batiment b(MAISON_X, MAISON_Z, 3.0f); /* 3 m sur 3 m de demi-largeur : 45 degrés */
    REQUIRE_THAT(b.sommet, WithinAbs(109.0f, 1e-3));
    for (const Vertex& s : b.v) {
        if (std::fabs(s.position.y - 109.0f) < 1e-3f && s.normal.y > 0.01f) {
            REQUIRE_THAT(s.position.z, WithinAbs(3.0f, 1e-3)); /* faîtage au milieu */
        }
    }
    /* Les pignons montent jusqu'au faîtage : pas de jour sous le toit. */
    REQUIRE_THAT(b.hautDesMurs(), WithinAbs(109.0f, 1e-3));
}

TEST_CASE("Toit à deux pans : pente minimale selon le matériau", "[toit]") {
    /* Hauteur inconnue : pente minimale, 30 degrés, ou 40 pour l'ardoise. */
    REQUIRE_THAT(Batiment(MAISON_X, MAISON_Z, -1.0f).sommet, WithinAbs(106.0f + 3.0f * 0.5774f, 1e-3));
    REQUIRE_THAT(Batiment(MAISON_X, MAISON_Z, -1.0f, true).sommet,
                 WithinAbs(106.0f + 3.0f * 0.8391f, 1e-3));
}

TEST_CASE("Toit plat quand la BD TOPO le dit", "[toit]") {
    const Batiment b(MAISON_X, MAISON_Z, 0.2f);
    REQUIRE_THAT(b.sommet, WithinAbs(106.0f, 1e-3));
    REQUIRE_THAT(b.surfaceToitPlat(), WithinAbs(60.0f, 1e-2));
}

TEST_CASE("Toit plat sur un immeuble, même étroit", "[toit]") {
    REQUIRE_THAT(Batiment(MAISON_X, MAISON_Z, 3.0f, false, 20.0f).sommet,
                 WithinAbs(120.0f, 1e-3));
}

TEST_CASE("Toit plat d'une emprise en L : ni débord ni trou", "[toit]") {
    /* L de 10 x 10 privé d'un carré de 5 x 5 : 75 m². Depuis le sommet 0,
       un éventail couvrirait aussi le carré manquant (100 m²). */
    const std::vector<float> px{0.0f, 0.0f, 10.0f, 10.0f, 5.0f, 5.0f};
    const std::vector<float> pz{10.0f, 0.0f, 0.0f, 5.0f, 5.0f, 10.0f};
    const Batiment           b(px, pz, 3.0f); /* trop loin du rectangle : plat */
    REQUIRE_THAT(b.sommet, WithinAbs(106.0f, 1e-3));
    REQUIRE_THAT(b.surfaceToitPlat(), WithinAbs(75.0f, 1e-2));
}

TEST_CASE("Emprise trouée : la cour reste ouverte, son mur la regarde", "[toit]") {
    /* Carré de 10 m percé d'une cour de 4 m (2 à 6), relié au contour par un
       pont, comme l'écrit tools/fetch_buildings.py : 100 - 16 = 84 m². */
    const std::vector<float> px{0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 2.0f, 2.0f, 6.0f, 10.0f, 0.0f};
    const std::vector<float> pz{0.0f, 0.0f, 10.0f, 6.0f, 2.0f, 2.0f, 6.0f, 6.0f, 10.0f, 10.0f};
    const Batiment           b(px, pz, 3.0f);
    REQUIRE_THAT(b.sommet, WithinAbs(106.0f, 1e-3)); /* trop loin du rectangle : plat */
    REQUIRE_THAT(b.surfaceToitPlat(), WithinAbs(84.0f, 1e-2));

    /* Mur ouest de la cour (x = 2) : sa normale pointe vers la cour, +x. */
    bool vu = false;
    for (const Vertex& s : b.v) {
        if (std::fabs(s.normal.y) < 0.01f && std::fabs(s.normal.z) < 0.01f &&
            std::fabs(s.position.x - 2.0f) < 1e-4f) {
            REQUIRE(s.normal.x > 0.99f);
            vu = true;
        }
    }
    REQUIRE(vu);
}
