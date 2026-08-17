/*
 * relief_emboitement_tests.cpp
 * La fenêtre de relief fin ne doit pas REDESSINER la surface du maillage
 * d'ensemble : elle doit la REPRODUIRE. Trois propriétés portent cela, et une
 * seule qui casse ramène le défaut des silhouettes à la frontière.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"
#include "render/relief/FenetreRelief.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using artouste::render::interpolerTriangle;
using artouste::render::relief::emboiteDansMaille;
using Catch::Matchers::WithinAbs;

namespace {

/* Surface du maillage d'ensemble sur une maille unique, coins quelconques. */
constexpr float H00 = 100.0f;
constexpr float H10 = 130.0f;
constexpr float H01 = 110.0f;
constexpr float H11 = 190.0f;

}  // namespace

TEST_CASE("l'interpolation suit l'anti-diagonale, pas la bilinéaire", "[relief]") {
    /* Au centre de la maille, l'anti-diagonale passe par les deux coins qu'elle
       relie : la hauteur y vaut leur moyenne. La bilinéaire, elle, donnerait la
       moyenne des QUATRE coins. C'est le seul point où les deux se séparent de
       façon lisible, et c'est la signature de la bonne diagonale. */
    const float centre = interpolerTriangle(H00, H10, H01, H11, 0.5f, 0.5f);
    REQUIRE_THAT(centre, WithinAbs(0.5f * (H10 + H01), 1e-4f));

    const float bilineaire = 0.25f * (H00 + H10 + H01 + H11);
    REQUIRE(std::fabs(centre - bilineaire) > 1.0f);

    /* Les quatre coins sont rendus exactement, quelle que soit la diagonale. */
    REQUIRE_THAT(interpolerTriangle(H00, H10, H01, H11, 0.0f, 0.0f), WithinAbs(H00, 1e-4f));
    REQUIRE_THAT(interpolerTriangle(H00, H10, H01, H11, 1.0f, 0.0f), WithinAbs(H10, 1e-4f));
    REQUIRE_THAT(interpolerTriangle(H00, H10, H01, H11, 0.0f, 1.0f), WithinAbs(H01, 1e-4f));
    REQUIRE_THAT(interpolerTriangle(H00, H10, H01, H11, 1.0f, 1.0f), WithinAbs(H11, 1e-4f));
}

TEST_CASE("une grille emboîtée reproduit la surface du maillage à l'exact", "[relief]") {
    /* La fenêtre pose ses sommets tous les 1/n de maille et les relie par ses
       propres triangles, de MÊME diagonale. Chaque sous-maille tombe alors
       entière dans un triangle du maillage, où la surface est linéaire : une
       interpolation linéaire d'une fonction linéaire est exacte.

       C'est la propriété qui rend la frontière de la fenêtre invisible. */
    const auto maillage = [](float x, float z) {
        return interpolerTriangle(H00, H10, H01, H11, x, z);
    };
    const auto fenetre = [&maillage](float x, float z, int n) {
        const float pas = 1.0f / static_cast<float>(n);
        const float xi  = std::floor(x / pas) * pas;
        const float zi  = std::floor(z / pas) * pas;
        return interpolerTriangle(maillage(xi, zi), maillage(xi + pas, zi),
                                  maillage(xi, zi + pas), maillage(xi + pas, zi + pas),
                                  (x - xi) / pas, (z - zi) / pas);
    };

    for (int n : {2, 4, 8}) {
        for (int a = 1; a < 20; ++a) {
            for (int b = 1; b < 20; ++b) {
                const float x = static_cast<float>(a) / 20.0f;
                const float z = static_cast<float>(b) / 20.0f;
                REQUIRE_THAT(fenetre(x, z, n), WithinAbs(maillage(x, z), 1e-3f));
            }
        }
    }
}

TEST_CASE("le pas de l'anneau doit s'emboîter, pas seulement celui du noyau", "[relief]") {
    using artouste::render::relief::PAS_ANNEAU;

    /* bigorre : maille de carte 8,7592 m, pas visé 2,1898 m, soit un quart. */
    REQUIRE(emboiteDansMaille(8.7592f / 4.0f, 8.7592f, PAS_ANNEAU));

    /* Le piège : un pas qui divise la maille pour le NOYAU mais pas pour
       l'ANNEAU. 8,7592 / 5 emboîte le noyau, et l'anneau tombe à côté. */
    REQUIRE(emboiteDansMaille(8.7592f / 5.0f, 8.7592f, 1));
    REQUIRE_FALSE(emboiteDansMaille(8.7592f / 5.0f, 8.7592f, PAS_ANNEAU));

    /* Le pas rond de 2 m, celui d'avant l'emboîtement : il ne divise pas. */
    REQUIRE_FALSE(emboiteDansMaille(2.0f, 8.7592f, PAS_ANNEAU));

    /* Valeurs absurdes : refusées plutôt que tolérées. */
    REQUIRE_FALSE(emboiteDansMaille(0.0f, 8.7592f, PAS_ANNEAU));
    REQUIRE_FALSE(emboiteDansMaille(2.0f, 0.0f, PAS_ANNEAU));
}
