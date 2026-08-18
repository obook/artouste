/*
 * BuildingsGeometrie.cpp
 * Extrusion d'une emprise (voir BuildingsGeometrie.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/buildings/BuildingsGeometrie.hpp"

#include <algorithm>
#include <cmath>

namespace artouste::render {

namespace {

/* Taille réelle de la tuile de façade (assets/textures/facade.png, mêmes
   valeurs dans tools/facade/generer_facade.py). Les UV des murs sont en mètres
   réels divisés par ces tailles : la texture se pose à la même échelle sur un
   pavillon et sur un immeuble, sans dépendre du nombre de sommets. */
constexpr float FACADE_TILE_W_M = 12.0f;
constexpr float FACADE_TILE_H_M = 6.0f;

/* Au-delà de 45 degrés, la face ne regarde plus dans l'axe de référence. */
constexpr float COS_45 = 0.70710678f;

void pushTriangle(std::vector<Vertex>&       verts,
                  std::vector<unsigned int>& idx,
                  const vec3&                a,
                  const vec3&                b,
                  const vec3&                c,
                  const vec3&                normal,
                  const vec3&                color) {
    const auto base = static_cast<unsigned int>(verts.size());
    verts.push_back(Vertex{a, normal, color, {0.0f, 0.0f}});
    verts.push_back(Vertex{b, normal, color, {0.0f, 0.0f}});
    verts.push_back(Vertex{c, normal, color, {0.0f, 0.0f}});
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
}

/* Quad d'un mur (bi, bj en bas ; ti, tj en haut). Un u1 négatif demande la
   tuile aveugle plutôt que la tuile fenêtrée ; la répétition, elle, est
   indifférente au signe. */
void pushWallQuad(std::vector<Vertex>&       verts,
                  std::vector<unsigned int>& idx,
                  const vec3&                bi,
                  const vec3&                bj,
                  const vec3&                tj,
                  const vec3&                ti,
                  const vec3&                normal,
                  const vec3&                color,
                  float                      u1,
                  float                      vTop) {
    const auto base = static_cast<unsigned int>(verts.size());
    verts.push_back(Vertex{bi, normal, color, {0.0f, 0.0f}});
    verts.push_back(Vertex{bj, normal, color, {u1, 0.0f}});
    verts.push_back(Vertex{tj, normal, color, {u1, vTop}});
    verts.push_back(Vertex{ti, normal, color, {0.0f, vTop}});
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
}

/* Normale du côté le plus long de l'emprise. Elle sert d'axe de référence pour
   décider quelles faces sont fenêtrées : un bâtiment réel montre ses fenêtres
   sur ses deux longues faces et garde ses pignons pleins. */
[[nodiscard]] vec3 normaleDeReference(const std::vector<float>& px,
                                      const std::vector<float>& pz) {
    const std::size_t n       = px.size();
    float             refNx   = 1.0f;
    float             refNz   = 0.0f;
    float             refLen2 = -1.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j    = (i + 1) % n;
        const float       ex   = px[j] - px[i];
        const float       ez   = pz[j] - pz[i];
        const float       len2 = ex * ex + ez * ez;
        if (len2 > refLen2) {
            refLen2         = len2;
            const float inv = 1.0f / std::sqrt(std::max(len2, 1e-6f));
            refNx           = ez * inv;
            refNz           = -ex * inv;
        }
    }
    return vec3{refNx, 0.0f, refNz};
}

} /* namespace */

void extruderBatiment(std::vector<Vertex>&       verts,
                      std::vector<unsigned int>& idx,
                      const std::vector<float>&  px,
                      const std::vector<float>&  pz,
                      float                      cx,
                      float                      cz,
                      float                      base,
                      float                      hauteur,
                      const vec3&                mur,
                      const vec3&                toit) {
    const std::size_t n   = px.size();
    const float       top = base + hauteur;

    /* Deux faces opposées ont des normales opposées : le critère est pris en
       valeur absolue, sinon elles recevraient des habillages différents
       (fenêtres d'un côté, mur nu en face).

       Le résultat voyage jusqu'au fragment par le SIGNE de l'UV horizontal :
       la structure Vertex est partagée avec le terrain et son million de
       sommets, lui ajouter un attribut coûterait 4 Mo par carte. Voir
       building.frag. */
    const vec3  ref  = normaleDeReference(px, pz);
    const float vTop = hauteur / FACADE_TILE_H_M;

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j  = (i + 1) % n;
        const float       ex = px[j] - px[i];
        const float       ez = pz[j] - pz[i];

        /* Normale horizontale perpendiculaire au côté, orientée vers
           l'extérieur (à l'opposé du centre de l'emprise). */
        float       nx = ez;
        float       nz = -ex;
        const float mx = 0.5f * (px[i] + px[j]) - cx;
        const float mz = 0.5f * (pz[i] + pz[j]) - cz;
        if (nx * mx + nz * mz < 0.0f) {
            nx = -nx;
            nz = -nz;
        }
        const vec3 normal = glm::normalize(vec3{nx, 0.0f, nz});

        const float sideLen  = std::sqrt(ex * ex + ez * ez);
        const bool  fenetree = std::fabs(normal.x * ref.x + normal.z * ref.z) >= COS_45;
        const float u1       = (fenetree ? 1.0f : -1.0f) * sideLen / FACADE_TILE_W_M;

        pushWallQuad(verts, idx, vec3{px[i], base, pz[i]}, vec3{px[j], base, pz[j]},
                     vec3{px[j], top, pz[j]}, vec3{px[i], top, pz[i]}, normal, mur, u1, vTop);
    }

    const vec3 up{0.0f, 1.0f, 0.0f};
    for (std::size_t i = 1; i + 1 < n; ++i) {
        pushTriangle(verts, idx, vec3{px[0], top, pz[0]}, vec3{px[i], top, pz[i]},
                     vec3{px[i + 1], top, pz[i + 1]}, up, toit);
    }
}

} /* namespace artouste::render */
