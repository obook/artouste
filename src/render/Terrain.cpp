/*
 * Terrain.cpp
 * Construit le maillage du terrain à partir de la carte d'altitude et drape
 * l'orthophoto. Les altitudes sont aussi gardées en mémoire pour répondre à
 * heightAt() (contact avec le sol, placement de l'appareil).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"

#include "render/Primitives.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace artouste::render {

namespace {

/*
 * Lit le fichier de calage terrain.txt (lignes "clé valeur", # = commentaire)
 * et range les valeurs attendues. Renvoie faux si une clé manque.
 */
bool readMetadata(const std::filesystem::path& path, int& cols, int& rows, float& widthM,
                  float& heightM, float& elevMin, float& elevMax, bool& drawSea,
                  bool& hasStart, float& startX, float& startZ, bool& hasGeo, float& lonMin,
                  float& lonMax, float& latMin, float& latMax) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    bool hasCols = false, hasRows = false, hasW = false, hasH = false, hasMin = false,
         hasMax = false;
    bool hasStartX = false, hasStartZ = false;
    bool hasLonMin = false, hasLonMax = false, hasLatMin = false, hasLatMax = false;
    std::string key;
    while (file >> key) {
        if (!key.empty() && key[0] == '#') {
            std::getline(file, key);  /* on jette le reste de la ligne de commentaire */
            continue;
        }
        if (key == "cols") {
            file >> cols, hasCols = true;
        } else if (key == "rows") {
            file >> rows, hasRows = true;
        } else if (key == "width_m") {
            file >> widthM, hasW = true;
        } else if (key == "height_m") {
            file >> heightM, hasH = true;
        } else if (key == "elev_min") {
            file >> elevMin, hasMin = true;
        } else if (key == "elev_max") {
            file >> elevMax, hasMax = true;
        } else if (key == "sea") {  /* 0 = pas de plan de mer (terrain de montagne) */
            int v = 1;
            file >> v;
            drawSea = (v != 0);
        } else if (key == "start_x") {
            file >> startX, hasStartX = true;
        } else if (key == "start_z") {
            file >> startZ, hasStartZ = true;
        } else if (key == "lon_min") {
            file >> lonMin, hasLonMin = true;
        } else if (key == "lon_max") {
            file >> lonMax, hasLonMax = true;
        } else if (key == "lat_min") {
            file >> latMin, hasLatMin = true;
        } else if (key == "lat_max") {
            file >> latMax, hasLatMax = true;
        } else {
            std::getline(file, key);  /* clé ignorée : on saute sa valeur */
        }
    }
    hasStart = hasStartX && hasStartZ;
    hasGeo   = hasLonMin && hasLonMax && hasLatMin && hasLatMax;
    return hasCols && hasRows && hasW && hasH && hasMin && hasMax;
}

}  /* namespace */

Terrain::Terrain(const std::filesystem::path& dir) {
    const std::filesystem::path meta   = dir / "terrain.txt";
    const std::filesystem::path height = dir / "heightmap.png";
    const std::filesystem::path ortho  = dir / "ortho.jpg";

    /* Lieux remarquables et hélipads du terrain (facultatifs : absent = aucun). */
    loadPlaces(dir / "landmarks.txt", m_landmarks, "lieu(x) remarquable(s)");
    loadPlaces(dir / "helipads.txt", m_helipads, "hélipad(s)");

    if (!readMetadata(meta, m_cols, m_rows, m_widthM, m_heightM, m_elevMin, m_elevMax,
                      m_drawSea, m_hasStart, m_startX, m_startZ, m_hasGeo, m_lonMin, m_lonMax,
                      m_latMin, m_latMax)) {
        std::fprintf(stderr, "[Terrain] calage absent (%s), repli sur un sol plat.\n",
                     meta.string().c_str());
        buildFlatFallback();
        return;
    }

    /* Lecture de la carte d'altitude en 16 bits, sans retournement vertical :
       on garde la rangée 0 au nord, comme l'a écrite l'outil de préparation. */
    stbi_set_flip_vertically_on_load(0);
    int             w = 0, h = 0, channels = 0;
    unsigned short* pixels = stbi_load_16(height.string().c_str(), &w, &h, &channels, 1);
    if (pixels == nullptr || w != m_cols || h != m_rows) {
        std::fprintf(stderr, "[Terrain] heightmap illisible ou de taille inattendue (%s).\n",
                     height.string().c_str());
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        buildFlatFallback();
        return;
    }

    /* Reconstitution des altitudes en mètres à partir des niveaux de gris. */
    const float span = m_elevMax - m_elevMin;
    m_heights.resize(static_cast<std::size_t>(m_cols) * static_cast<std::size_t>(m_rows));
    for (std::size_t k = 0; k < m_heights.size(); ++k) {
        m_heights[k] = m_elevMin + (static_cast<float>(pixels[k]) / 65535.0f) * span;
    }
    stbi_image_free(pixels);

    /* Plateformes d'hélipad : on aplanit le relief sous le point de départ et sous
       chaque hélipad, pour que le sol et l'appareil posé s'accordent à une même
       hauteur (sinon, sur une maille en pente, l'appareil s'enfonce ou se pose en
       travers). À faire avant de construire le maillage, qui en hérite. */
    flattenPads();

    /* --- Construction du maillage du relief ---------------------------------- */
    const float halfW = 0.5f * m_widthM;
    const float halfH = 0.5f * m_heightM;
    const float dx    = m_widthM / static_cast<float>(m_cols - 1);   /* pas est-ouest (m) */
    const float dz    = m_heightM / static_cast<float>(m_rows - 1);  /* pas nord-sud (m) */

    primitives::MeshData data;
    data.vertices.reserve(m_heights.size());
    data.indices.reserve(static_cast<std::size_t>(m_cols - 1) * static_cast<std::size_t>(m_rows - 1)
                         * 6);

    /* Indice linéaire d'un point (colonne i, rangée j) dans la grille. */
    const auto idx = [cols = m_cols](int i, int j) -> std::size_t {
        return static_cast<std::size_t>(j) * static_cast<std::size_t>(cols)
               + static_cast<std::size_t>(i);
    };

    const vec3 white{1.0f, 1.0f, 1.0f};  /* la couleur vient de la texture */
    for (int j = 0; j < m_rows; ++j) {
        for (int i = 0; i < m_cols; ++i) {
            const float x = -halfW + static_cast<float>(i) * dx;
            const float z = -halfH + static_cast<float>(j) * dz;  /* rangée 0 = nord (Z min) */
            const float y = m_heights[idx(i, j)];

            /* Normale par différences finies sur le relief (voisins bornés au bord). */
            const int   iL = i > 0 ? i - 1 : i;
            const int   iR = i < m_cols - 1 ? i + 1 : i;
            const int   jU = j > 0 ? j - 1 : j;
            const int   jD = j < m_rows - 1 ? j + 1 : j;
            const float hL = m_heights[idx(iL, j)];
            const float hR = m_heights[idx(iR, j)];
            const float hU = m_heights[idx(i, jU)];
            const float hD = m_heights[idx(i, jD)];
            const float dydx   = (hR - hL) / (static_cast<float>(iR - iL) * dx);
            const float dydz   = (hD - hU) / (static_cast<float>(jD - jU) * dz);
            const vec3  normal = glm::normalize(vec3{-dydx, 1.0f, -dydz});

            Vertex v;
            v.position = vec3{x, y, z};
            v.normal   = normal;
            v.color    = white;
            v.uv       = vec2{static_cast<float>(i) / static_cast<float>(m_cols - 1),
                            1.0f - static_cast<float>(j) / static_cast<float>(m_rows - 1)};
            data.vertices.push_back(v);
        }
    }

    for (int j = 0; j < m_rows - 1; ++j) {
        for (int i = 0; i < m_cols - 1; ++i) {
            const unsigned int a = static_cast<unsigned int>(j * m_cols + i);
            const unsigned int b = a + 1;
            const unsigned int c = a + static_cast<unsigned int>(m_cols);
            const unsigned int d = c + 1;
            data.indices.insert(data.indices.end(), {a, c, b, b, c, d});
        }
    }

    m_mesh = Mesh(data.vertices, data.indices);

    m_ortho    = Texture(ortho);
    m_textured = m_ortho.valid();
    if (!m_textured) {
        std::fprintf(stderr, "[Terrain] orthophoto absente (%s), relief sans texture.\n",
                     ortho.string().c_str());
    } else {
        std::printf("[Terrain] terrain chargé : %.0f x %.0f m, altitude max %.0f m.\n",
                    static_cast<double>(m_widthM), static_cast<double>(m_heightM),
                    static_cast<double>(m_elevMax));
    }
}

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

float Terrain::heightAt(float x, float z) const noexcept {
    if (m_heights.empty()) {
        return 0.0f;
    }
    const float halfW = 0.5f * m_widthM;
    const float halfH = 0.5f * m_heightM;
    if (x < -halfW || x > halfW || z < -halfH || z > halfH) {
        return 0.0f;  /* hors emprise : on est au-dessus de la mer */
    }

    /* Coordonnées fractionnaires dans la grille (colonne = est, rangée = sud). */
    const float fx = (x + halfW) / m_widthM * static_cast<float>(m_cols - 1);
    const float fz = (z + halfH) / m_heightM * static_cast<float>(m_rows - 1);
    const int   i0 = static_cast<int>(fx);
    const int   j0 = static_cast<int>(fz);
    const int   i1 = i0 < m_cols - 1 ? i0 + 1 : i0;
    const int   j1 = j0 < m_rows - 1 ? j0 + 1 : j0;
    const float tx = fx - static_cast<float>(i0);
    const float tz = fz - static_cast<float>(j0);

    const auto at = [this](int i, int j) {
        return m_heights[static_cast<std::size_t>(j) * static_cast<std::size_t>(m_cols)
                         + static_cast<std::size_t>(i)];
    };
    const float top    = lerp(at(i0, j0), at(i1, j0), tx);
    const float bottom = lerp(at(i0, j1), at(i1, j1), tx);
    return lerp(top, bottom, tz);
}

}  /* namespace artouste::render */
