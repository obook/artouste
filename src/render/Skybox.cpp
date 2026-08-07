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
    /* Le triangle plein écran se calcule entièrement dans sky.vert à partir de
       gl_VertexID, sans le moindre attribut de sommet : on pourrait donc se
       contenter d'un VAO sans rien y attacher. Certains pilotes (Mesa V3D du
       Raspberry Pi 5, confirmé en test) refusent pourtant de dessiner un VAO
       sans aucun tableau de sommets actif (GL_INVALID_VALUE, rien ne s'affiche
       -- et ni le ciel ni RIEN d'autre ensuite, tout le reste de la scène
       sombre alors dans le noir). On attache donc un VBO factice, jamais lu
       par le shader, à seule fin de satisfaire ces pilotes. */
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    constexpr float factice[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    glBufferData(GL_ARRAY_BUFFER, sizeof(factice), factice, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);
}

Skybox::~Skybox() {
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
}

void Skybox::draw(Shader& shader, const mat4& invRotProj, const vec3& sunDir, const vec3& moonDir,
                  float timeSeconds) const {
    shader.use();
    shader.setMat4("u_invViewProj", invRotProj);
    shader.setVec3("u_sunDir", sunDir);
    shader.setVec3("u_moonDir", moonDir);
    shader.setFloat("u_time", timeSeconds);

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
