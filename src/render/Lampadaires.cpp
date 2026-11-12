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
/* Hauteur du luminaire au-dessus de la chaussée, et d'un feu de piste. */
constexpr float HAUTEUR_M = 8.0f;
constexpr float HAUTEUR_FEU_M = 0.5f;
} /* namespace */

Lampadaires::Lampadaires(const std::filesystem::path& dir, const Terrain& terrain,
                         const std::vector<vec4>& feux) {
    std::ifstream in(dir / "lampadaires.bin", std::ios::binary);
    char          magie[4] = {};
    std::uint32_t version  = 0;
    std::uint32_t nombre   = 0;
    in.read(magie, 4);
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&nombre), sizeof(nombre));
    if (!in || std::string_view(magie, 4) != "ALMP" || (version != 1u && version != 2u)) {
        if (in.is_open()) { /* absent : pas d'éclairage public pour ce terrain */
            std::fprintf(stderr, "[Lampadaires] %s : en-tête invalide, éclairage ignoré.\n",
                         (dir / "lampadaires.bin").string().c_str());
        }
        nombre = 0;
    }

    /* Par point : x, y, z et le type (0 rue, 1 feu d'aérodrome), lu tel quel par
       le shader (a_pos.w). La version 1 n'a pas d'octet de type : tout est rue. */
    std::vector<float> points;
    points.reserve(nombre * 4u);
    for (std::uint32_t k = 0; k < nombre; ++k) {
        float        lon = 0.0f, lat = 0.0f;
        std::uint8_t type = 0;
        in.read(reinterpret_cast<char*>(&lon), sizeof(lon));
        in.read(reinterpret_cast<char*>(&lat), sizeof(lat));
        if (version == 2u) {
            in.read(reinterpret_cast<char*>(&type), sizeof(type));
        }
        if (!in) {
            break;
        }
        float x = 0.0f, z = 0.0f;
        terrain.worldAt(lon, lat, x, z);
        const float hauteur = type == 0 ? HAUTEUR_M : HAUTEUR_FEU_M;
        points.insert(points.end(), {x, terrain.heightAt(x, z) + hauteur, z, float(type)});
    }
    for (const vec4& f : feux) {
        points.insert(points.end(), {f.x, f.y, f.z, f.w});
    }
    if (points.empty()) {
        return;
    }

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(points.size() * sizeof(float)),
                 points.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_count = points.size() / 4u;
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
