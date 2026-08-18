/*
 * TerrainPads.cpp
 * Plates-formes des hélipads : un plateau posé au point le plus haut du relief.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>

namespace artouste::render {

Terrain::~Terrain() {
    if (m_carteRelief != 0) {
        glDeleteTextures(1, &m_carteRelief);
    }
}

void Terrain::buildPadPlatforms() {
    /* Hauteur du plateau = point le plus HAUT du relief sous l'emprise du pad
       (centre + deux anneaux d'échantillons), lu AVANT d'enregistrer la
       plate-forme (les appels à heightAt ne sont donc pas influencés par elle).
       Caler sur le seul centre laissait, sur un terrain en pente, le relief
       amont crever le disque ; calé sur le maximum, le disque coiffe tout le
       relief de son emprise et la jupe habille le côté aval. Les pads hors
       emprise sont ignorés, comme au dessin. */
    if (!m_hasGeo) {
        return;
    }
    constexpr float TWO_PI = 6.2831853f;
    constexpr int SAMPLES = 16; /* par anneau : assez serré pour des mailles de ~17 m */
    const float halfW = 0.5f * m_widthM;
    const float halfH = 0.5f * m_heightM;
    m_padPlatforms.reserve(m_helipads.size());
    for (const Landmark& pad : m_helipads) {
        float x = 0.0f, z = 0.0f;
        worldAt(pad.lon, pad.lat, x, z);
        /* worldAt() renvoie des coordonnées MONDE ; l'emprise [-halfW,halfW] est
           locale (centrée sur m_originX/m_originZ) : sans la soustraction, un pad
           hors du centre de la carte (recadrée) est cru hors emprise et n'obtient
           jamais sa plate-forme anti-enfoncement. */
        if (std::fabs(x - m_originX) > halfW || std::fabs(z - m_originZ) > halfH) {
            continue;
        }
        float top = heightAt(x, z);
        for (int ring = 1; ring <= 2; ++ring) {
            const float r = PAD_PLATFORM_RADIUS_M * static_cast<float>(ring) / 2.0f;
            for (int i = 0; i < SAMPLES; ++i) {
                const float a = TWO_PI * static_cast<float>(i) / static_cast<float>(SAMPLES);
                top = std::fmax(top, heightAt(x + r * std::cos(a), z + r * std::sin(a)));
            }
        }
        m_padPlatforms.push_back({x, z, top});
    }
}

} /* namespace artouste::render */
