/*
 * TerrainMaillage.cpp
 * Construction du maillage du relief à partir de la carte d'altitude.
 *
 * La carte peut être plus fine que ce qu'on veut dessiner : on n'en retient
 * qu'un point sur n, le plus petit pas qui tienne dans le budget de sommets.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"

#include "render/Primitives.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace artouste::render {

void Terrain::construireMaillage(int sommetsMax) {
    const float halfW = 0.5f * m_widthM;
    const float halfH = 0.5f * m_heightM;
    const float dx = m_widthM / static_cast<float>(m_cols - 1);  /* pas est-ouest (m) */
    const float dz = m_heightM / static_cast<float>(m_rows - 1); /* pas nord-sud (m) */

    /* Points de grille RETENUS pour le maillage. La carte d'altitude peut être
       plus fine que ce qu'on veut dessiner : doubler sa finesse quadruple le
       nombre de triangles, ce qu'un GPU intégré ne suit pas, alors que les
       altitudes elles-mêmes restent utiles en entier pour heightAt (poser,
       plates-formes, collision). On échantillonne donc le relief pour le dessin
       sans rien jeter pour la physique.

       Le dernier point de chaque axe est toujours retenu, quitte à raccourcir la
       dernière maille : les positions se déduisent de l'indice réel, la
       géométrie reste donc exacte jusqu'au bord de l'emprise. */
    const auto retenus = [](int nb, int pas) {
        std::vector<int> pris;
        pris.reserve(static_cast<std::size_t>(nb / pas + 2));
        for (int k = 0; k < nb - 1; k += pas) {
            pris.push_back(k);
        }
        pris.push_back(nb - 1);
        return pris;
    };
    /* Pas d'échantillonnage : le plus petit qui tienne dans le budget de
       sommets. Un pas de s divise leur nombre par s au carré. */
    int pasMaillage = 1;
    if (sommetsMax > 0) {
        const double total = static_cast<double>(m_cols) * static_cast<double>(m_rows);
        pasMaillage = std::max(1, static_cast<int>(std::ceil(
                                      std::sqrt(total / static_cast<double>(sommetsMax)))));
    }
    m_pasMaillage = pasMaillage;
    const std::vector<int> colonnes    = retenus(m_cols, pasMaillage);
    const std::vector<int> rangees     = retenus(m_rows, pasMaillage);
    if (pasMaillage > 1) {
        std::printf("[Terrain] maillage allégé : 1 point sur %d, %zu x %zu sommets "
                    "(maille %.0f m au lieu de %.0f m).\n",
                    pasMaillage,
                    colonnes.size(),
                    rangees.size(),
                    static_cast<double>(dx * static_cast<float>(pasMaillage)),
                    static_cast<double>(dx));
    }

    primitives::MeshData data;
    data.vertices.reserve(colonnes.size() * rangees.size());
    data.indices.reserve((colonnes.size() - 1) * (rangees.size() - 1) * 6);

    /* Indice linéaire d'un point (colonne i, rangée j) dans la grille. */
    const auto idx = [cols = m_cols](int i, int j) -> std::size_t {
        return static_cast<std::size_t>(j) * static_cast<std::size_t>(cols) +
               static_cast<std::size_t>(i);
    };

    const vec3 white{1.0f, 1.0f, 1.0f}; /* la couleur vient de la texture */

    /* Pas du gradient des normales : environ 35 m de part et d'autre, quelle
       que soit la finesse de la grille. Au pas d'une seule maille fine
       (17,5 m), chaque micro-facette accrochait la lumière et les versants
       lointains scintillaient en quadrillage régulier ; on lisse l'ÉCLAIRAGE
       sans rien enlever au relief lui-même. */
    const int step = std::max(1, static_cast<int>(std::lround(35.0f / dx)));
    m_pasNormaleM  = static_cast<float>(step) * dx;
    std::printf("[Terrain] maillage d'ensemble : 1 point sur %d, normales à %.2f m.\n",
                m_pasMaillage, static_cast<double>(m_pasNormaleM));

    for (const int j : rangees) {
        for (const int i : colonnes) {
            const float x = m_originX - halfW + static_cast<float>(i) * dx;
            const float z =
                m_originZ - halfH + static_cast<float>(j) * dz; /* rangée 0 = nord (Z min) */
            const float y = m_heights[idx(i, j)];

            /* Normale par différences finies sur le relief (voisins bornés au bord). */
            const int iL = std::max(0, i - step);
            const int iR = std::min(m_cols - 1, i + step);
            const int jU = std::max(0, j - step);
            const int jD = std::min(m_rows - 1, j + step);
            const float hL = m_heights[idx(iL, j)];
            const float hR = m_heights[idx(iR, j)];
            const float hU = m_heights[idx(i, jU)];
            const float hD = m_heights[idx(i, jD)];
            const float dydx = (hR - hL) / (static_cast<float>(iR - iL) * dx);
            const float dydz = (hD - hU) / (static_cast<float>(jD - jU) * dz);
            const vec3 normal = glm::normalize(vec3{-dydx, 1.0f, -dydz});

            Vertex v;
            v.position = vec3{x, y, z};
            v.normal = normal;
            v.color = white;
            v.uv = vec2{static_cast<float>(i) / static_cast<float>(m_cols - 1),
                        1.0f - static_cast<float>(j) / static_cast<float>(m_rows - 1)};
            data.vertices.push_back(v);
        }
    }

    /* Les indices portent sur les sommets RETENUS, pas sur la grille d'origine. */
    const auto largeurMaillage = static_cast<unsigned int>(colonnes.size());
    for (unsigned int j = 0; j + 1 < rangees.size(); ++j) {
        for (unsigned int i = 0; i + 1 < largeurMaillage; ++i) {
            const unsigned int a = j * largeurMaillage + i;
            const unsigned int b = a + 1;
            const unsigned int c = a + largeurMaillage;
            const unsigned int d = c + 1;
            data.indices.insert(data.indices.end(), {a, c, b, b, c, d});
        }
    }

    m_mesh = Mesh(data.vertices, data.indices);
}

} /* namespace artouste::render */
