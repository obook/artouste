/*
 * BuildingsDraw.cpp
 * Culling par frustum et par distance de brume des tuiles de bâtiments, puis
 * dessin des tuiles visibles. La lecture de buildings.bin et la construction
 * du maillage sont dans BuildingsMesh.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Buildings.hpp"

#include <algorithm>
#include <cstdlib>

namespace artouste::render {

/* Extrait les 6 plans du frustum (méthode Gribb-Hartmann) d'une matrice monde -> clip.
   Chaque plan (a,b,c,d) : un point (x,y,z) est du bon côté si a*x+b*y+c*z+d >= 0. Pas
   de normalisation : seul le signe importe pour le test boîte/frustum. */
namespace {

void extractFrustum(const mat4& m, vec4 planes[6]) {
    /* GLM est en colonnes majeures : m[col][row]. Les "lignes" de la matrice sont
       donc (m[0][r], m[1][r], m[2][r], m[3][r]). */
    const vec4 r0{m[0][0], m[1][0], m[2][0], m[3][0]};
    const vec4 r1{m[0][1], m[1][1], m[2][1], m[3][1]};
    const vec4 r2{m[0][2], m[1][2], m[2][2], m[3][2]};
    const vec4 r3{m[0][3], m[1][3], m[2][3], m[3][3]};
    planes[0] = r3 + r0; /* gauche  */
    planes[1] = r3 - r0; /* droite  */
    planes[2] = r3 + r1; /* bas     */
    planes[3] = r3 - r1; /* haut    */
    planes[4] = r3 + r2; /* proche  */
    planes[5] = r3 - r2; /* lointain*/
}

/* Vrai si la boîte [mn, mx] est entièrement du mauvais côté d'au moins un plan (donc
   hors du frustum). On teste le "sommet positif" de la boîte pour chaque plan : le coin
   le plus avancé dans le sens de la normale. S'il est déjà dehors, toute la boîte l'est. */
bool boxOutsideFrustum(const vec4 planes[6], const vec3& mn, const vec3& mx) {
    for (int p = 0; p < 6; ++p) {
        const vec4& pl = planes[p];
        const float vx = (pl.x >= 0.0f) ? mx.x : mn.x;
        const float vy = (pl.y >= 0.0f) ? mx.y : mn.y;
        const float vz = (pl.z >= 0.0f) ? mx.z : mn.z;
        if (pl.x * vx + pl.y * vy + pl.z * vz + pl.w < 0.0f) {
            return true;
        }
    }
    return false;
}

} /* namespace */

void Buildings::drawTabliers() const {
    /* Pas de culling : quelques centaines de rubans plats, un seul appel de
       dessin, à comparer aux dizaines de milliers de bâtiments d'à-côté. */
    m_tabliers.draw();
}

void Buildings::draw(const mat4& worldViewProj, const vec3& camWorldPos) const {
    if (m_tiles.empty() || m_mesh.empty()) {
        return;
    }

    /* Diagnostic : ARTOUSTE_NO_CULL dessine tout le maillage d'un coup (culling
       désactivé), pour mesurer le gain du culling par tuiles. */
    static const bool noCull = std::getenv("ARTOUSTE_NO_CULL") != nullptr;
    if (noCull) {
        m_mesh.draw();
        return;
    }

    /* Distance de culling : au-delà, un bâtiment est entièrement noyé dans la brume
       (u_fogEnd du shader de bâtiments), donc invisible. Doit rester alignée sur FOG_END
       (app/AppConstants.hpp) ; on ne dessine pas les tuiles dont le point le plus proche
       dépasse cette distance. */
    constexpr float FAR_CULL_M = 22000.0f;
    constexpr float FAR_CULL_M2 = FAR_CULL_M * FAR_CULL_M;

    vec4 planes[6];
    extractFrustum(worldViewProj, planes);

    for (const Tile& t : m_tiles) {
        /* Hors champ ? */
        if (boxOutsideFrustum(planes, t.mn, t.mx)) {
            continue;
        }
        /* Point de la boîte le plus proche de la caméra : si déjà au-delà de la brume,
           toute la tuile l'est. */
        const float cx = std::max(t.mn.x, std::min(camWorldPos.x, t.mx.x));
        const float cy = std::max(t.mn.y, std::min(camWorldPos.y, t.mx.y));
        const float cz = std::max(t.mn.z, std::min(camWorldPos.z, t.mx.z));
        const float dx = cx - camWorldPos.x, dy = cy - camWorldPos.y, dz = cz - camWorldPos.z;
        if (dx * dx + dy * dy + dz * dz > FAR_CULL_M2) {
            continue;
        }
        m_mesh.drawRange(t.firstIndex, t.indexCount);
    }
}

} /* namespace artouste::render */
