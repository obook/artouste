/*
 * ZombieEyes.cpp
 * Voir ZombieEyes.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/combat/ZombieEyes.hpp"

#include <glad/glad.h>

#include <algorithm>

namespace artouste::render {

ZombieEyes::ZombieEyes(std::size_t capacity) : m_capacity(capacity) {
    /* Un seul quad, réorienté vers la caméra par le shader : une lueur ronde n'a
       pas d'orientation propre (même choix que Projectiles). */
    const float quad[] = {
        /* corner.x corner.y   u    v */
        -0.5f, -0.5f, 0.0f, 0.0f,
        0.5f, -0.5f, 1.0f, 0.0f,
        0.5f, 0.5f, 1.0f, 1.0f,
        -0.5f, 0.5f, 0.0f, 1.0f,
    };
    const unsigned int indices[] = {0, 1, 2, 0, 2, 3};

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    constexpr GLsizei vstride = 4 * sizeof(float);
    glGenBuffers(1, &m_quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vstride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vstride,
                          reinterpret_cast<void*>(2 * sizeof(float)));

    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    /* Tampon d'instances : centre + rayon (location 2), couleur (location 3). */
    constexpr GLsizei istride = static_cast<GLsizei>(sizeof(Instance));
    glGenBuffers(1, &m_instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_capacity * sizeof(Instance)), nullptr,
                GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, istride, reinterpret_cast<void*>(0));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, istride, reinterpret_cast<void*>(sizeof(vec4)));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_built = true;
}

void ZombieEyes::release() noexcept {
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
    m_count                                   = 0;
    m_built                                   = false;
}

ZombieEyes::~ZombieEyes() {
    release();
}

void ZombieEyes::updateInstances(const std::vector<Instance>& instances) {
    if (!m_built) {
        return;
    }
    m_count = std::min(instances.size(), m_capacity);
    if (m_count == 0) {
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(m_count * sizeof(Instance)),
                    instances.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ZombieEyes::draw() const {
    if (!m_built || m_count == 0) {
        return;
    }
    /* Additif, profondeur lue mais pas écrite : les lueurs s'occultent derrière
       le relief et l'appareil, se cumulent entre elles quand deux zombies se
       superposent, et n'empêchent rien d'être dessiné après elles. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glBindVertexArray(m_vao);
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(m_count));
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

}  /* namespace artouste::render */
