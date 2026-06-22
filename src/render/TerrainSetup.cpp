/*
 * TerrainSetup.cpp
 * Opérations annexes du terrain, appelées à la construction : chargement des lieux
 * remarquables et des hélipads (fichiers texte), aplanissement du relief sous les
 * aires de poser, et sol plat de secours en l'absence de données réelles. Le calage,
 * le maillage et heightAt() sont dans Terrain.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"

#include "render/Primitives.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace artouste::render {

void Terrain::loadPlaces(const std::filesystem::path& path, std::vector<Landmark>& out,
                         const char* label) {
    /* Format : un lieu par ligne "lon lat nom", le nom étant le reste de la ligne
       (il peut contenir des espaces). Ligne vide ou commençant par # ignorée. */
    std::ifstream file(path);
    if (!file) {
        return;  /* fichier absent pour ce terrain : tableau vide */
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        Landmark           lm;
        if (!(iss >> lm.lon >> lm.lat)) {
            continue;  /* ligne vide, commentaire ou mal formée */
        }
        std::getline(iss, lm.name);
        const std::size_t first = lm.name.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue;  /* coordonnées sans nom : on ignore */
        }
        const std::size_t last = lm.name.find_last_not_of(" \t\r\n");
        lm.name                = lm.name.substr(first, last - first + 1);
        out.push_back(std::move(lm));
    }
    std::printf("[Terrain] %zu %s chargé(s).\n", out.size(), label);
}

void Terrain::flattenPads() {
    if (m_heights.empty() || m_cols < 2 || m_rows < 2) {
        return;
    }
    /* Rayon aplani : le disque (7 m) plus la longueur de l'appareil, pour qu'il
       repose à plat même posé un peu décalé sur le pad. */
    constexpr float PAD_RADIUS_M = 12.0f;
    if (m_hasStart) {
        const float colf = (m_startX / m_widthM + 0.5f) * static_cast<float>(m_cols - 1);
        const float rowf = (m_startZ / m_heightM + 0.5f) * static_cast<float>(m_rows - 1);
        flattenAround(colf, rowf, PAD_RADIUS_M);
    }
    if (m_hasGeo) {
        for (const Landmark& pad : m_helipads) {
            const float colf = (pad.lon - m_lonMin) / (m_lonMax - m_lonMin)
                               * static_cast<float>(m_cols - 1);
            const float rowf = (m_latMax - pad.lat) / (m_latMax - m_latMin)
                               * static_cast<float>(m_rows - 1);
            flattenAround(colf, rowf, PAD_RADIUS_M);
        }
    }
}

void Terrain::flattenAround(float colf, float rowf, float radiusM) {
    if (colf < 0.0f || colf > static_cast<float>(m_cols - 1) || rowf < 0.0f
        || rowf > static_cast<float>(m_rows - 1)) {
        return;  /* hélipad hors de l'emprise : rien à aplanir */
    }
    const auto H = [&](int c, int r) -> float& {
        return m_heights[static_cast<std::size_t>(r) * static_cast<std::size_t>(m_cols)
                         + static_cast<std::size_t>(c)];
    };

    /* Hauteur cible : altitude interpolée au centre du pad, lue avant modification. */
    const int   c0 = std::clamp(static_cast<int>(std::floor(colf)), 0, m_cols - 2);
    const int   r0 = std::clamp(static_cast<int>(std::floor(rowf)), 0, m_rows - 2);
    const float fc = colf - static_cast<float>(c0);
    const float fr = rowf - static_cast<float>(r0);
    const float target = (H(c0, r0) * (1.0f - fc) + H(c0 + 1, r0) * fc) * (1.0f - fr)
                         + (H(c0, r0 + 1) * (1.0f - fc) + H(c0 + 1, r0 + 1) * fc) * fr;

    /* Rayon converti en nombre de cellules sur chaque axe (la maille est plus
       large nord-sud qu'est-ouest), puis on met à plat tous les noeuds couverts. */
    const float dCol = radiusM / (m_widthM / static_cast<float>(m_cols - 1));
    const float dRow = radiusM / (m_heightM / static_cast<float>(m_rows - 1));
    const int   cmin = std::clamp(static_cast<int>(std::floor(colf - dCol)), 0, m_cols - 1);
    const int   cmax = std::clamp(static_cast<int>(std::ceil(colf + dCol)), 0, m_cols - 1);
    const int   rmin = std::clamp(static_cast<int>(std::floor(rowf - dRow)), 0, m_rows - 1);
    const int   rmax = std::clamp(static_cast<int>(std::ceil(rowf + dRow)), 0, m_rows - 1);
    for (int r = rmin; r <= rmax; ++r) {
        for (int c = cmin; c <= cmax; ++c) {
            H(c, r) = target;
        }
    }
}

void Terrain::buildFlatFallback() {
    /* Damier plat de secours, identique à l'ancien sol, en cas de données absentes. */
    m_cols = m_rows = 0;
    m_widthM = m_heightM = 800.0f;
    m_elevMin = m_elevMax = 0.0f;
    m_heights.clear();
    m_textured = false;

    const primitives::MeshData data =
        primitives::groundGrid(400.0f, 80, vec3{0.33f, 0.50f, 0.24f}, vec3{0.29f, 0.45f, 0.21f});
    m_mesh = Mesh(data.vertices, data.indices);
}

}  /* namespace artouste::render */
