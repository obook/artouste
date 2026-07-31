/*
 * SouffleFx.cpp
 * Voir SouffleFx.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/SouffleFx.hpp"

#include <glad/glad.h>

#include <algorithm>

namespace artouste::render {

SouffleFx::SouffleFx(std::size_t capacite) : m_capacite(capacite) {
    /* Un seul quad, orienté face caméra par le shader : une bouffée est un
       nuage informe, rien ne justifie la croix de la végétation. */
    /* clang-format off */
    const float quad[] = {
        /* corner.x corner.y   u    v */
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
    };
    /* clang-format on */
    const unsigned int indices[] = {0, 1, 2, 0, 2, 3};

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    constexpr GLsizei vstride = 4 * sizeof(float);
    glGenBuffers(1, &m_quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vstride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, vstride, reinterpret_cast<void*>(2 * sizeof(float)));

    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    /* Tampon d'instances : attribut 2 (centre + diamètre) et 3 (opacité, graine,
       rotation, hauteur sol), soit huit flottants par bouffée. */
    constexpr GLsizei istride = static_cast<GLsizei>(sizeof(Instance));
    glGenBuffers(1, &m_instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_capacite * sizeof(Instance)),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, istride, nullptr);
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, istride, reinterpret_cast<void*>(sizeof(vec4)));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_built = true;
}

void SouffleFx::release() noexcept {
    if (m_instanceVbo != 0) {
        glDeleteBuffers(1, &m_instanceVbo);
    }
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
    }
    if (m_quadVbo != 0) {
        glDeleteBuffers(1, &m_quadVbo);
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    m_vao = m_quadVbo = m_ebo = m_instanceVbo = 0;
    m_count = 0;
    m_built = false;
}

SouffleFx::~SouffleFx() {
    release();
}

void SouffleFx::updateInstances(const std::vector<Instance>& instances) {
    if (!m_built) {
        return;
    }
    m_count = std::min(instances.size(), m_capacite);
    if (m_count == 0) {
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferSubData(
        GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(m_count * sizeof(Instance)), instances.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SouffleFx::draw() const {
    if (!m_built || m_count == 0) {
        return;
    }
    glBindVertexArray(m_vao);
    glDrawElementsInstanced(
        GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(m_count));
    glBindVertexArray(0);
}

} /* namespace artouste::render */
