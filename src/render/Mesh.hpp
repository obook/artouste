// Maillage indexé minimal : VAO/VBO/EBO gérés en RAII (move-only).
// Sommet = position + normale + couleur, suffisant pour le rendu M1.

#pragma once

#include "util/Math.hpp"

#include <vector>

namespace artouste::render {

struct Vertex {
    vec3 position;
    vec3 normal;
    vec3 color;
    vec2 uv{0.0f, 0.0f};  // coordonnées de texture (0 pour la géométrie procédurale)
};

class Mesh {
public:
    Mesh() = default;
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw() const;

    [[nodiscard]] bool empty() const noexcept { return m_indexCount == 0; }

private:
    void release() noexcept;

    unsigned int m_vao        = 0;
    unsigned int m_vbo        = 0;
    unsigned int m_ebo        = 0;
    int          m_indexCount = 0;
};

}  // namespace artouste::render
