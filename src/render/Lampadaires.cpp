/*
 * Lampadaires.cpp
 * Éclairage public en points lumineux (voir Lampadaires.hpp).
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#include "render/Lampadaires.hpp"

#include "render/Terrain.hpp"
#include "util/Math.hpp"

#include <glad/glad.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string_view>
#include <vector>

namespace artouste::render {

namespace {
/* Hauteur du luminaire au-dessus de la chaussée. */
constexpr float HAUTEUR_M = 8.0f;
} /* namespace */

Lampadaires::Lampadaires(const std::filesystem::path& dir, const Terrain& terrain) {
    std::ifstream in(dir / "lampadaires.bin", std::ios::binary);
    if (!in) {
        return; /* pas d'éclairage public pour ce terrain */
    }
    char          magie[4] = {};
    std::uint32_t version  = 0;
    std::uint32_t nombre   = 0;
    in.read(magie, 4);
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&nombre), sizeof(nombre));
    if (!in || std::string_view(magie, 4) != "ALMP" || version != 1u) {
        std::fprintf(stderr, "[Lampadaires] %s : en-tête invalide, éclairage ignoré.\n",
                     (dir / "lampadaires.bin").string().c_str());
        return;
    }

    std::vector<vec3> points;
    points.reserve(nombre);
    for (std::uint32_t k = 0; k < nombre; ++k) {
        float lon = 0.0f, lat = 0.0f;
        in.read(reinterpret_cast<char*>(&lon), sizeof(lon));
        in.read(reinterpret_cast<char*>(&lat), sizeof(lat));
        if (!in) {
            break;
        }
        float x = 0.0f, z = 0.0f;
        terrain.worldAt(lon, lat, x, z);
        points.push_back(vec3{x, terrain.heightAt(x, z) + HAUTEUR_M, z});
    }
    if (points.empty()) {
        return;
    }

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(points.size() * sizeof(vec3)),
                 points.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), nullptr);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_count = points.size();
    std::printf("[Lampadaires] %zu points d'éclairage public.\n", m_count);
}

Lampadaires::~Lampadaires() {
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
}

void Lampadaires::draw() const {
    if (m_count == 0) {
        return;
    }
    /* Additif sans écriture de profondeur, mais avec le test : un immeuble
       cache les rues derrière lui. La taille du point vient du shader. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_count));
    glBindVertexArray(0);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} /* namespace artouste::render */
