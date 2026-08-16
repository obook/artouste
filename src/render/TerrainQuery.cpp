/*
 * TerrainQuery.cpp
 * Requêtes runtime sur le terrain déjà construit : altitude du sol en un
 * point (heightAt) et recherche de la balise HAPI la plus proche
 * (hapiUnitNear). Le calage, le maillage et la construction des plates-formes
 * d'hélipad sont dans Terrain.cpp ; le chargement des fichiers annexes et
 * l'aplanissement du relief dans TerrainSetup.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"

#include <cmath>

namespace artouste::render {

float Terrain::heightAt(float x, float z) const noexcept {
    float h = heightCoarse(x, z);

    /* Fenêtre de relief fin : ce qui est dessiné sous l'appareil fait foi. Elle
       n'apporte qu'un détail, le relief d'ensemble gardant ses basses
       fréquences, comme dans terrain.vert. */
    if (m_relief) {
        float detail = 0.0f;
        float poids  = 0.0f;
        if (m_relief->detailEn(x, z, detail, poids)) {
            h += poids * detail;
        }
    }

    /* Plates-formes d'hélisurface : dans leur emprise, le sol porteur ne descend
       jamais sous le plateau du pad. Un pad perché (sommet du pic du Midi d'Ossau)
       porte ainsi l'appareil sans déformer le relief alentour. */
    for (const PadPlatform& pad : m_padPlatforms) {
        const float dx = x - pad.x;
        const float dz = z - pad.z;
        if (dx * dx + dz * dz <= PAD_PLATFORM_RADIUS_M * PAD_PLATFORM_RADIUS_M && pad.top > h) {
            h = pad.top;
        }
    }
    return h;
}

float Terrain::heightCoarse(float x, float z) const noexcept {
    if (m_heights.empty()) {
        return 0.0f;
    }
    const float halfW = 0.5f * m_widthM;
    const float halfH = 0.5f * m_heightM;
    /* Coordonnées locales, l'emprise étant centrée sur (m_originX, m_originZ). */
    const float lx = x - m_originX;
    const float lz = z - m_originZ;
    if (lx < -halfW || lx > halfW || lz < -halfH || lz > halfH) {
        return 0.0f; /* hors emprise : on est au-dessus de la mer */
    }

    /* Coordonnées fractionnaires dans la grille (colonne = est, rangée = sud). */
    const float fx = (lx + halfW) / m_widthM * static_cast<float>(m_cols - 1);
    const float fz = (lz + halfH) / m_heightM * static_cast<float>(m_rows - 1);
    const int i0 = static_cast<int>(fx);
    const int j0 = static_cast<int>(fz);
    const int i1 = i0 < m_cols - 1 ? i0 + 1 : i0;
    const int j1 = j0 < m_rows - 1 ? j0 + 1 : j0;
    const float tx = fx - static_cast<float>(i0);
    const float tz = fz - static_cast<float>(j0);

    const auto at = [this](int i, int j) {
        return m_heights[static_cast<std::size_t>(j) * static_cast<std::size_t>(m_cols) +
                         static_cast<std::size_t>(i)];
    };

    /* Même découpe que le maillage rendu : chaque cellule est faite de deux
     * triangles séparés par la diagonale b-c (voir la construction des indices).
     * L'interpolation doit suivre ces triangles et non la surface bilinéaire,
     * sinon l'appareil posé s'enfonce dans le relief partout où la surface
     * bilinéaire passe sous les triangles (sensible sur les fortes pentes). */
    const float ha = at(i0, j0);
    const float hb = at(i1, j0);
    const float hc = at(i0, j1);
    const float hd = at(i1, j1);
    float h = 0.0f;
    if (tx + tz <= 1.0f) {
        h = ha + tx * (hb - ha) + tz * (hc - ha); /* triangle a-c-b */
    } else {
        h = hd + (1.0f - tx) * (hc - hd) + (1.0f - tz) * (hb - hd); /* triangle b-c-d */
    }
    return h;
}

const HapiUnit* Terrain::hapiUnitNear(float lon, float lat, float maxDistM) const noexcept {
    if (!m_hasGeo || m_hapiUnits.empty()) {
        return nullptr;
    }
    float px = 0.0f, pz = 0.0f;
    worldAt(lon, lat, px, pz);
    const HapiUnit* best = nullptr;
    float bestDist = maxDistM;
    for (const HapiUnit& hapi : m_hapiUnits) {
        float hx = 0.0f, hz = 0.0f;
        worldAt(hapi.lon, hapi.lat, hx, hz);
        const float dist = std::sqrt((hx - px) * (hx - px) + (hz - pz) * (hz - pz));
        if (dist <= bestDist) {
            best = &hapi;
            bestDist = dist;
        }
    }
    return best;
}

} /* namespace artouste::render */
