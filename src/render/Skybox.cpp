/*
 * Skybox.cpp
 * Affiche le ciel : on dessine un grand triangle au fond de l'image et
 * on désactive l'écriture de profondeur pour qu'il reste derrière tous
 * les autres objets.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Skybox.hpp"

#include <glad/glad.h>

#include "render/Shader.hpp"

namespace artouste::render {

Skybox::Skybox() {
    glGenVertexArrays(1, &m_vao);
}

Skybox::~Skybox() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
}

void Skybox::draw(Shader& shader, const mat4& invViewProj, const vec3& camPos) const {
    shader.use();
    shader.setMat4("u_invViewProj", invViewProj);
    shader.setVec3("u_camPos", camPos);

    /*
     * On désactive le test et l'écriture de profondeur le temps de
     * tracer le ciel, puis on les rétablit pour le reste de la scène.
     */
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

}  /* namespace artouste::render */
