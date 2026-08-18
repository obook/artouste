/*
 * SkinnedZombies.cpp
 * Voir SkinnedZombies.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/combat/SkinnedZombies.hpp"

#include "render/combat/SkinnedZombiesReglages.hpp"

#include "render/Shader.hpp"
#include "render/Texture.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>

namespace artouste::render {



SkinnedZombies::SkinnedZombies(const std::filesystem::path& modelPath, std::size_t capacity,
                               int phaseGroups)
    : m_model(modelPath), m_capacity(capacity), m_phaseGroups(std::max(1, phaseGroups)) {
    if (!m_model.built()) {
        std::fprintf(stderr, "[SkinnedZombies] pack absent/illisible (%s) : zombies ignorés.\n",
                     modelPath.string().c_str());
        return;
    }

    const std::size_t variants = m_model.meshCount();
    const auto        stride    = static_cast<GLsizei>(FLOATS_PER_INSTANCE * sizeof(float));
    const auto        vtxStride = static_cast<GLsizei>(sizeof(SkinnedModel::SkinnedVertex));

    for (std::size_t v = 0; v < variants; ++v) {
        const SkinnedModel::MeshData& md = m_model.mesh(v);
        Part                          part;
        part.indexCount = static_cast<int>(md.indices.size());
        part.boneCount  = static_cast<int>(md.boneNode.size());

        glGenVertexArrays(1, &part.vao);
        glBindVertexArray(part.vao);

        /* Sommets skinnés (statiques). */
        glGenBuffers(1, &part.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, part.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(md.vertices.size() * sizeof(SkinnedModel::SkinnedVertex)),
                     md.vertices.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &part.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, part.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(md.indices.size() * sizeof(unsigned int)),
                     md.indices.data(), GL_STATIC_DRAW);

        /* position, normale, uv (flottants), puis os (entiers) et poids. */
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vtxStride, reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vtxStride,
                              reinterpret_cast<void*>(offsetof(SkinnedModel::SkinnedVertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, vtxStride,
                              reinterpret_cast<void*>(offsetof(SkinnedModel::SkinnedVertex, uv)));
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_INT, vtxStride,
                               reinterpret_cast<void*>(offsetof(SkinnedModel::SkinnedVertex, joints)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, vtxStride,
                              reinterpret_cast<void*>(offsetof(SkinnedModel::SkinnedVertex, weights)));

        /* Tampon d'instances (dynamique) : mat4 aux locations 5-8, flash en 9. */
        glGenBuffers(1, &part.instanceVbo);
        glBindBuffer(GL_ARRAY_BUFFER, part.instanceVbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m_capacity * FLOATS_PER_INSTANCE * sizeof(float)),
                     nullptr, GL_DYNAMIC_DRAW);
        for (int col = 0; col < 4; ++col) {
            const auto loc = static_cast<unsigned int>(5 + col);
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, stride,
                                  reinterpret_cast<void*>(static_cast<std::size_t>(col) * sizeof(vec4)));
            glVertexAttribDivisor(loc, 1);
        }
        glEnableVertexAttribArray(9);
        glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(mat4)));
        glVertexAttribDivisor(9, 1);
        /* Graine de couleur (location 10) : juste après le flash. */
        glEnableVertexAttribArray(10);
        glVertexAttribPointer(10, 1, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(sizeof(mat4) + sizeof(float)));
        glVertexAttribDivisor(10, 1);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        m_parts.push_back(part);
    }

    /* Décalages de phase répartis sur toute la durée de l'animation. */
    m_phaseOffset.resize(static_cast<std::size_t>(m_phaseGroups));
    const float duration = m_model.durationS();
    for (int g = 0; g < m_phaseGroups; ++g) {
        m_phaseOffset[static_cast<std::size_t>(g)] =
            duration * static_cast<float>(g) / static_cast<float>(m_phaseGroups);
    }

    m_buckets.resize(variants * static_cast<std::size_t>(m_phaseGroups));

    /* Ancrages d'yeux de la pose de repos (t=0 décalé du groupe) : draw() les
       rafraîchit ensuite à chaque image pour les lots posés. Les calculer ici
       évite qu'une lueur parte de l'origine du monde à la toute première image,
       avant le premier draw(). */
    m_eyePoints.assign(m_buckets.size(), {vec3(0.0f), vec3(0.0f)});
    const auto G = static_cast<std::size_t>(m_phaseGroups);
    for (std::size_t g = 0; g < G; ++g) {
        const float             tg      = m_phaseOffset[g];
        const std::vector<mat4> globals = m_model.poseAtTime(tg);
        for (std::size_t v = 0; v < variants; ++v) {
            storeEyePoints(v * G + g, v, posedBones(v, globals, tg));
        }
    }

    m_built = !m_parts.empty();
    if (m_built) {
        std::printf("[SkinnedZombies] %zu variantes, %d groupes de phase, capacité %zu.\n",
                    m_parts.size(), m_phaseGroups, m_capacity);
    }
}

void SkinnedZombies::release() noexcept {
    for (Part& p : m_parts) {
        if (p.instanceVbo != 0) glDeleteBuffers(1, &p.instanceVbo);
        if (p.vbo != 0) glDeleteBuffers(1, &p.vbo);
        if (p.ebo != 0) glDeleteBuffers(1, &p.ebo);
        if (p.vao != 0) glDeleteVertexArrays(1, &p.vao);
    }
    m_parts.clear();
    m_built = false;
}

SkinnedZombies::~SkinnedZombies() {
    release();
}

} /* namespace artouste::render */
