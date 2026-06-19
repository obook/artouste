#include "render/Mesh.hpp"

#include <glad/glad.h>

#include <cstddef>
#include <utility>

namespace artouste::render {

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    : m_indexCount(static_cast<int>(indices.size())) {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, uv)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Mesh::release() noexcept {
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    m_vao        = 0;
    m_vbo        = 0;
    m_ebo        = 0;
    m_indexCount = 0;
}

Mesh::~Mesh() {
    release();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_vao(std::exchange(other.m_vao, 0)),
      m_vbo(std::exchange(other.m_vbo, 0)),
      m_ebo(std::exchange(other.m_ebo, 0)),
      m_indexCount(std::exchange(other.m_indexCount, 0)) {}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        release();
        m_vao        = std::exchange(other.m_vao, 0);
        m_vbo        = std::exchange(other.m_vbo, 0);
        m_ebo        = std::exchange(other.m_ebo, 0);
        m_indexCount = std::exchange(other.m_indexCount, 0);
    }
    return *this;
}

void Mesh::draw() const {
    if (m_indexCount == 0) {
        return;
    }
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

}  // namespace artouste::render
