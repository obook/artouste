/*
 * Primitives.cpp
 * Implémente la fabrication des formes géométriques de base utilisées
 * par le simulateur : boîte, damier au sol, jeu de pales et disque.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Primitives.hpp"

#include <cmath>

namespace artouste::render::primitives {

namespace {

/*
 * Ajoute un quadrilatère (deux triangles) à partir de quatre sommets
 * d'un même plan et d'une normale commune. L'ordre des sommets importe
 * peu ici : on dessine les deux faces, seule la normale sert à
 * l'éclairage.
 */
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

/*
 * Recopie une forme dans une autre en lui appliquant un déplacement et
 * une rotation (la matrice xform). Sert par exemple à placer chaque pale
 * autour du moyeu.
 */
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

}  /* namespace */

MeshData box(const vec3& h, const vec3& color) {
    MeshData out;
    out.vertices.reserve(24);
    out.indices.reserve(36);

    /* Les six faces de la boîte, une paire par axe. */
    /* +X / -X */
    addQuad(out, {h.x, -h.y, -h.z}, {h.x, -h.y, h.z}, {h.x, h.y, h.z}, {h.x, h.y, -h.z},
            {1.0f, 0.0f, 0.0f}, color);
    addQuad(out, {-h.x, -h.y, h.z}, {-h.x, -h.y, -h.z}, {-h.x, h.y, -h.z}, {-h.x, h.y, h.z},
            {-1.0f, 0.0f, 0.0f}, color);
    /* +Y / -Y */
    addQuad(out, {-h.x, h.y, -h.z}, {h.x, h.y, -h.z}, {h.x, h.y, h.z}, {-h.x, h.y, h.z},
            {0.0f, 1.0f, 0.0f}, color);
    addQuad(out, {-h.x, -h.y, h.z}, {h.x, -h.y, h.z}, {h.x, -h.y, -h.z}, {-h.x, -h.y, -h.z},
            {0.0f, -1.0f, 0.0f}, color);
    /* +Z / -Z */
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

    /* Une pale modèle, décalée vers l'extérieur le long de l'axe +X. */
    MeshData blade = box({length * 0.5f, thickness * 0.5f, chord * 0.5f}, color);
    for (Vertex& v : blade.vertices) {
        v.position.x += length * 0.5f + hubRadius;
    }

    /* On répartit les pales en les faisant tourner régulièrement autour de Y. */
    for (int k = 0; k < count; ++k) {
        const float angle = static_cast<float>(k) * (TWO_PI / static_cast<float>(count));
        const mat4  rot   = glm::rotate(mat4(1.0f), angle, vec3{0.0f, 1.0f, 0.0f});
        appendTransformed(out, blade, rot);
    }

    /* Le moyeu central qui relie les pales. */
    appendTransformed(out, box({hubRadius, thickness * 1.5f, hubRadius}, color), mat4(1.0f));
    return out;
}

MeshData disc(float radius, int segments, const vec3& color) {
    MeshData   out;
    const vec3 up{0.0f, 1.0f, 0.0f};
    out.vertices.push_back({vec3{0.0f, 0.0f, 0.0f}, up, color});  /* centre */
    /* On place des points sur le cercle, reliés au centre pour former des parts de tarte. */
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

MeshData softDisc(float radius, int segments) {
    MeshData   out;
    const vec3 up{0.0f, 1.0f, 0.0f};
    /* Centre : facteur de bord 0 (canal rouge à 0). */
    out.vertices.push_back({vec3{0.0f, 0.0f, 0.0f}, up, vec3{0.0f, 0.0f, 0.0f}});
    /* Pourtour : facteur de bord 1 (canal rouge à 1), lu par le shader d'ombre. */
    const vec3 edge{1.0f, 1.0f, 1.0f};
    for (int i = 0; i <= segments; ++i) {
        const float a = static_cast<float>(i) * (TWO_PI / static_cast<float>(segments));
        out.vertices.push_back({vec3{radius * std::cos(a), 0.0f, radius * std::sin(a)}, up, edge});
    }
    for (int i = 1; i <= segments; ++i) {
        out.indices.push_back(0);
        out.indices.push_back(static_cast<unsigned int>(i));
        out.indices.push_back(static_cast<unsigned int>(i + 1));
    }
    return out;
}

MeshData sphere(float radius, int rings, int sectors, const vec3& color) {
    MeshData out;
    /* Grille de sommets en (latitude, longitude). La latitude va du pôle bas au
       pôle haut, la longitude fait le tour. La normale d'une sphère centrée sur
       l'origine est simplement la direction du sommet. */
    for (int r = 0; r <= rings; ++r) {
        const float phi = -HALF_PI + PI * static_cast<float>(r) / static_cast<float>(rings);
        for (int s = 0; s <= sectors; ++s) {
            const float theta = TWO_PI * static_cast<float>(s) / static_cast<float>(sectors);
            const vec3  n{std::cos(phi) * std::cos(theta), std::sin(phi),
                          std::cos(phi) * std::sin(theta)};
            out.vertices.push_back({n * radius, n, color});
        }
    }
    /* Deux triangles par case du quadrillage. */
    const int stride = sectors + 1;
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            const auto a = static_cast<unsigned int>(r * stride + s);
            const auto b = static_cast<unsigned int>((r + 1) * stride + s);
            out.indices.push_back(a);
            out.indices.push_back(b);
            out.indices.push_back(a + 1);
            out.indices.push_back(a + 1);
            out.indices.push_back(b);
            out.indices.push_back(b + 1);
        }
    }
    return out;
}

MeshData helipad(float radius, int segments, const vec3& padColor, const vec3& ringColor,
                 const vec3& letterColor) {
    MeshData   out;
    const vec3 up{0.0f, 1.0f, 0.0f};

    /* Le disque sombre de la plateforme, au ras du sol. */
    appendTransformed(out, disc(radius, segments, padColor), mat4(1.0f));

    /* On empile les marques claires juste au-dessus du disque pour qu'elles
       restent visibles sans bagarre de profondeur avec lui (z-fighting). */
    const float ringY = 0.02f;
    const float markY = 0.04f;

    /* Anneau clair près du bord : une couronne de quadrilatères entre deux rayons. */
    const float rIn  = radius * 0.86f;
    const float rOut = radius * 0.96f;
    for (int i = 0; i < segments; ++i) {
        const float a0 = static_cast<float>(i) * (TWO_PI / static_cast<float>(segments));
        const float a1 = static_cast<float>(i + 1) * (TWO_PI / static_cast<float>(segments));
        const vec3  in0{rIn * std::cos(a0), ringY, rIn * std::sin(a0)};
        const vec3  out0{rOut * std::cos(a0), ringY, rOut * std::sin(a0)};
        const vec3  out1{rOut * std::cos(a1), ringY, rOut * std::sin(a1)};
        const vec3  in1{rIn * std::cos(a1), ringY, rIn * std::sin(a1)};
        addQuad(out, in0, out0, out1, in1, up, ringColor);
    }

    /* Le grand H : deux montants verticaux (selon Z) reliés par une barre. */
    const float halfW  = radius * 0.26f;  /* demi-écart entre les montants (axe X) */
    const float halfL  = radius * 0.36f;  /* demi-hauteur des montants (axe Z) */
    const float stroke = radius * 0.10f;  /* épaisseur des barres */
    const auto  bar    = [&](float x0, float x1, float z0, float z1) {
        addQuad(out, {x0, markY, z0}, {x1, markY, z0}, {x1, markY, z1}, {x0, markY, z1}, up,
                letterColor);
    };
    bar(-halfW, -halfW + stroke, -halfL, halfL);          /* montant gauche */
    bar(halfW - stroke, halfW, -halfL, halfL);            /* montant droit */
    bar(-halfW + stroke, halfW - stroke, -stroke * 0.5f, stroke * 0.5f);  /* barre centrale */

    return out;
}

}  /* namespace artouste::render::primitives */
