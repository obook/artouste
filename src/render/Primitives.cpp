#include "render/Primitives.hpp"

#include <cmath>

namespace artouste::render::primitives {

namespace {

// Ajoute un quad (deux triangles) à partir de quatre sommets coplanaires et
// d'une normale partagée. Le sens d'enroulement importe peu : le rendu M1
// désactive le face culling, seule la normale sert à l'éclairage.
void addQuad(MeshData& out, const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3,
             const vec3& normal, const vec3& color) {
    const unsigned int base = static_cast<unsigned int>(out.vertices.size());
    out.vertices.push_back({p0, normal, color});
    out.vertices.push_back({p1, normal, color});
    out.vertices.push_back({p2, normal, color});
    out.vertices.push_back({p3, normal, color});
    out.indices.insert(out.indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
}

// Recopie une géométrie dans une autre en lui appliquant une transformation.
// On suppose xform sans mise à l'échelle non uniforme (rotation + translation).
void appendTransformed(MeshData& out, const MeshData& src, const mat4& xform) {
    const mat3         normalMat = mat3(xform);
    const unsigned int base      = static_cast<unsigned int>(out.vertices.size());
    for (const Vertex& v : src.vertices) {
        Vertex nv;
        nv.position = vec3(xform * vec4(v.position, 1.0f));
        nv.normal   = glm::normalize(normalMat * v.normal);
        nv.color    = v.color;
        out.vertices.push_back(nv);
    }
    for (const unsigned int idx : src.indices) {
        out.indices.push_back(base + idx);
    }
}

}  // namespace

MeshData box(const vec3& h, const vec3& color) {
    MeshData out;
    out.vertices.reserve(24);
    out.indices.reserve(36);

    // +X / -X
    addQuad(out, {h.x, -h.y, -h.z}, {h.x, -h.y, h.z}, {h.x, h.y, h.z}, {h.x, h.y, -h.z},
            {1.0f, 0.0f, 0.0f}, color);
    addQuad(out, {-h.x, -h.y, h.z}, {-h.x, -h.y, -h.z}, {-h.x, h.y, -h.z}, {-h.x, h.y, h.z},
            {-1.0f, 0.0f, 0.0f}, color);
    // +Y / -Y
    addQuad(out, {-h.x, h.y, -h.z}, {h.x, h.y, -h.z}, {h.x, h.y, h.z}, {-h.x, h.y, h.z},
            {0.0f, 1.0f, 0.0f}, color);
    addQuad(out, {-h.x, -h.y, h.z}, {h.x, -h.y, h.z}, {h.x, -h.y, -h.z}, {-h.x, -h.y, -h.z},
            {0.0f, -1.0f, 0.0f}, color);
    // +Z / -Z
    addQuad(out, {-h.x, -h.y, h.z}, {h.x, -h.y, h.z}, {h.x, h.y, h.z}, {-h.x, h.y, h.z},
            {0.0f, 0.0f, 1.0f}, color);
    addQuad(out, {h.x, -h.y, -h.z}, {-h.x, -h.y, -h.z}, {-h.x, h.y, -h.z}, {h.x, h.y, -h.z},
            {0.0f, 0.0f, -1.0f}, color);

    return out;
}

MeshData groundGrid(float halfSize, int cells, const vec3& colorA, const vec3& colorB) {
    MeshData    out;
    const float step = (2.0f * halfSize) / static_cast<float>(cells);
    const vec3  up{0.0f, 1.0f, 0.0f};

    for (int i = 0; i < cells; ++i) {
        for (int j = 0; j < cells; ++j) {
            const float x0    = -halfSize + static_cast<float>(i) * step;
            const float z0    = -halfSize + static_cast<float>(j) * step;
            const float x1    = x0 + step;
            const float z1    = z0 + step;
            const vec3  color = ((i + j) % 2 == 0) ? colorA : colorB;
            addQuad(out, {x0, 0.0f, z0}, {x1, 0.0f, z0}, {x1, 0.0f, z1}, {x0, 0.0f, z1}, up,
                    color);
        }
    }
    return out;
}

MeshData bladeSet(int count, float length, float chord, float thickness, const vec3& color) {
    MeshData    out;
    const float hubRadius = 0.20f;

    // Une pale de référence, décalée vers l'extérieur le long de +X.
    MeshData blade = box({length * 0.5f, thickness * 0.5f, chord * 0.5f}, color);
    for (Vertex& v : blade.vertices) {
        v.position.x += length * 0.5f + hubRadius;
    }

    for (int k = 0; k < count; ++k) {
        const float angle = static_cast<float>(k) * (TWO_PI / static_cast<float>(count));
        const mat4  rot   = glm::rotate(mat4(1.0f), angle, vec3{0.0f, 1.0f, 0.0f});
        appendTransformed(out, blade, rot);
    }

    appendTransformed(out, box({hubRadius, thickness * 1.5f, hubRadius}, color), mat4(1.0f));
    return out;
}

MeshData disc(float radius, int segments, const vec3& color) {
    MeshData   out;
    const vec3 up{0.0f, 1.0f, 0.0f};
    out.vertices.push_back({vec3{0.0f, 0.0f, 0.0f}, up, color});  // centre
    for (int i = 0; i <= segments; ++i) {
        const float a = static_cast<float>(i) * (TWO_PI / static_cast<float>(segments));
        out.vertices.push_back({vec3{radius * std::cos(a), 0.0f, radius * std::sin(a)}, up, color});
    }
    for (int i = 1; i <= segments; ++i) {
        out.indices.push_back(0);
        out.indices.push_back(static_cast<unsigned int>(i));
        out.indices.push_back(static_cast<unsigned int>(i + 1));
    }
    return out;
}

}  // namespace artouste::render::primitives
