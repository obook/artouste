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

/* Quad quelconque, avec sa normale et sans texture (UV nuls). Sert au tablier,
   dont aucune face n'est un mur de façade. */
void pushQuad(std::vector<Vertex>&       verts,
              std::vector<unsigned int>& idx,
              const vec3&                a,
              const vec3&                b,
              const vec3&                c,
              const vec3&                d,
              const vec3&                normal,
              const vec3&                color) {
    const auto base = static_cast<unsigned int>(verts.size());
    verts.push_back(Vertex{a, normal, color, {0.0f, 0.0f}});
    verts.push_back(Vertex{b, normal, color, {0.0f, 0.0f}});
    verts.push_back(Vertex{c, normal, color, {0.0f, 0.0f}});
    verts.push_back(Vertex{d, normal, color, {0.0f, 0.0f}});
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
}

/* Perpendiculaire horizontale à l'axe au point i. Aux points intérieurs on
   moyenne les deux segments voisins : sans cela le ruban montrerait une fente
   à chaque changement de direction. */
[[nodiscard]] vec3 perpendiculaireAxe(const std::vector<float>& px,
                                      const std::vector<float>& pz,
                                      std::size_t               i) {
    const std::size_t n = px.size();
    float             dx = 0.0f;
    float             dz = 0.0f;
    const auto ajouter = [&](std::size_t a, std::size_t b) {
        const float ex   = px[b] - px[a];
        const float ez   = pz[b] - pz[a];
        const float len2 = ex * ex + ez * ez;
        if (len2 > 1e-12f) {
            const float inv = 1.0f / std::sqrt(len2);
            dx += ex * inv;
            dz += ez * inv;
        }
    };
    if (i > 0) {
        ajouter(i - 1, i);
    }
    if (i + 1 < n) {
        ajouter(i, i + 1);
    }
    const float len2 = dx * dx + dz * dz;
    if (len2 < 1e-12f) {
        return vec3{1.0f, 0.0f, 0.0f}; /* axe dégénéré : direction arbitraire */
    }
    const float inv = 1.0f / std::sqrt(len2);
    return vec3{dz * inv, 0.0f, -dx * inv};
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

/* Quad de chaussée : même géométrie qu'un pushQuad, mais l'UV porte le drapage
   de l'orthophoto au lieu d'être nul. Dessiné avec le shader du terrain. */
void pushQuadChaussee(std::vector<Vertex>&       verts,
                      std::vector<unsigned int>& idx,
                      const vec3&                a,
                      const vec3&                b,
                      const vec3&                c,
                      const vec3&                d,
                      const vec3&                normal,
                      const CalageOrtho&         ortho) {
    const vec3 blanc{1.0f, 1.0f, 1.0f}; /* le shader du terrain ignore la couleur */
    const auto base = static_cast<unsigned int>(verts.size());
    verts.push_back(Vertex{a, normal, blanc, ortho.uv(a.x, a.z)});
    verts.push_back(Vertex{b, normal, blanc, ortho.uv(b.x, b.z)});
    verts.push_back(Vertex{c, normal, blanc, ortho.uv(c.x, c.z)});
    verts.push_back(Vertex{d, normal, blanc, ortho.uv(d.x, d.z)});
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
}

void extruderTablier(std::vector<Vertex>&       vertsBeton,
                     std::vector<unsigned int>& idxBeton,
                     std::vector<Vertex>&       vertsChaussee,
                     std::vector<unsigned int>& idxChaussee,
                     const std::vector<float>&  px,
                     const std::vector<float>&  py,
                     const std::vector<float>&  pz,
                     float                      largeur,
                     const vec3&                couleur,
                     const CalageOrtho&         ortho) {
    const std::size_t n = px.size();
    if (n < 2) {
        return;
    }
    const float demi = 0.5f * largeur;

    /* Les flancs sont verticaux dans la réalité, mais le shader trie mur et
       toit sur la seule normale (voir building.frag) : une normale horizontale
       leur collerait la texture de façade, fenêtres comprises. On les incline
       donc vers le haut, ce qui donne au passage un chanfrein plus proche d'un
       bord de tablier que d'une arête vive. */
    constexpr float FLANC_HORIZ = 0.8f;
    constexpr float FLANC_VERT  = 0.6f;
    const vec3      haut{0.0f, 1.0f, 0.0f};
    const vec3      bas{0.0f, -1.0f, 0.0f};

    for (std::size_t i = 0; i + 1 < n; ++i) {
        const std::size_t j = i + 1;
        const vec3        pi = perpendiculaireAxe(px, pz, i);
        const vec3        pj = perpendiculaireAxe(px, pz, j);

        const float basI = py[i] - EPAISSEUR_TABLIER_M;
        const float basJ = py[j] - EPAISSEUR_TABLIER_M;

        const vec3 hgI{px[i] + pi.x * demi, py[i], pz[i] + pi.z * demi};
        const vec3 hdI{px[i] - pi.x * demi, py[i], pz[i] - pi.z * demi};
        const vec3 hgJ{px[j] + pj.x * demi, py[j], pz[j] + pj.z * demi};
        const vec3 hdJ{px[j] - pj.x * demi, py[j], pz[j] - pj.z * demi};
        const vec3 bgI{hgI.x, basI, hgI.z};
        const vec3 bdI{hdI.x, basI, hdI.z};
        const vec3 bgJ{hgJ.x, basJ, hgJ.z};
        const vec3 bdJ{hdJ.x, basJ, hdJ.z};

        pushQuadChaussee(vertsChaussee, idxChaussee, hgI, hdI, hdJ, hgJ, haut, ortho);
        pushQuad(vertsBeton, idxBeton, bgI, bgJ, bdJ, bdI, bas, couleur * 0.75f);

        const vec3 nG = glm::normalize(vec3{pi.x * FLANC_HORIZ, FLANC_VERT, pi.z * FLANC_HORIZ});
        const vec3 nD = glm::normalize(vec3{-pi.x * FLANC_HORIZ, FLANC_VERT, -pi.z * FLANC_HORIZ});
        pushQuad(vertsBeton, idxBeton, hgI, hgJ, bgJ, bgI, nG, couleur);
        pushQuad(vertsBeton, idxBeton, hdI, bdI, bdJ, hdJ, nD, couleur);

        /* Capots aux deux bouts : la BD TOPO coupe le réseau à chaque
           intersection, un pont est donc rendu en plusieurs tronçons qui
           s'arrêtent net. Sans capot on voit à travers la tranche. */
        if (i == 0 || j + 1 == n) {
            /* Direction du segment, NORMALISÉE avant pondération : sans cela le
               poids vertical serait noyé par la longueur du segment et la
               normale repasserait à l'horizontale, donc en mur de façade. */
            const float ex   = px[j] - px[i];
            const float ez   = pz[j] - pz[i];
            const float len2 = ex * ex + ez * ez;
            const float inv  = (len2 > 1e-12f) ? 1.0f / std::sqrt(len2) : 0.0f;
            const vec3  axe{ex * inv * FLANC_HORIZ, FLANC_VERT, ez * inv * FLANC_HORIZ};
            if (i == 0) {
                pushQuad(vertsBeton, idxBeton, hgI, bgI, bdI, hdI,
                         glm::normalize(vec3{-axe.x, axe.y, -axe.z}), couleur);
            }
            if (j + 1 == n) {
                pushQuad(vertsBeton, idxBeton, hgJ, hdJ, bdJ, bgJ, glm::normalize(axe), couleur);
            }
        }
    }
}

} /* namespace artouste::render */
