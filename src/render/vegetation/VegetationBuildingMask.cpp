/*
 * VegetationBuildingMask.cpp
 * Masque d'emprise des bâtiments qui exclut leur toit du semis de végétation :
 * lit buildings.bin (mêmes emprises que render::Buildings) et rastérise
 * chaque polygone à la résolution de l'orthophoto. Le masque d'eau est dans
 * VegetationWaterMask.cpp, les zones d'exclusion dans VegetationMasks.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"
#include "render/Vegetation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>

namespace artouste::render {

std::vector<unsigned char> Vegetation::buildBuildingMask(const std::filesystem::path& terrainDir,
                                                         const Terrain& terrain,
                                                         int orthoW,
                                                         int orthoH,
                                                         float halfW,
                                                         float halfH) const {
    /* Masque des bâtiments : on ne plante pas d'arbre sur une emprise de bâtiment
       (sinon des arbres poussent sur les toits dans les villages, là où l'ortho est
       verte entre les maisons). On lit buildings.bin (mêmes emprises que render::
       Buildings), on rastérise chaque polygone dans un masque à la résolution de
       l'ortho, avec un pixel de marge pour que le billboard ne mange pas les murs.
       Format : magie "ABLD", version, nombre, puis par bâtiment hauteur(float),
       n(uint16), n x (lon,lat float). */
    std::vector<unsigned char> building(
        static_cast<std::size_t>(orthoW) * static_cast<std::size_t>(orthoH), 0);
    std::ifstream bf(terrainDir / "buildings.bin", std::ios::binary);
    char magic[4] = {0, 0, 0, 0};
    std::uint32_t ver = 0, count = 0;
    if (bf) {
        bf.read(magic, 4);
        bf.read(reinterpret_cast<char*>(&ver), sizeof(ver));
        bf.read(reinterpret_cast<char*>(&count), sizeof(count));
    }
    const bool ok =
        bf && magic[0] == 'A' && magic[1] == 'B' && magic[2] == 'L' && magic[3] == 'D' && ver == 1u;
    std::vector<float> bx, by; /* emprise en coordonnées pixel (réutilisé par bâtiment) */
    for (std::uint32_t bi = 0; ok && bi < count; ++bi) {
        float height = 0.0f;
        std::uint16_t npts = 0;
        bf.read(reinterpret_cast<char*>(&height), sizeof(height));
        bf.read(reinterpret_cast<char*>(&npts), sizeof(npts));
        if (!bf || npts < 3) {
            break; /* fichier tronqué ou emprise dégénérée */
        }
        bx.clear();
        by.clear();
        float cmin = 1e9f, cmax = -1e9f, rmin = 1e9f, rmax = -1e9f;
        for (std::uint16_t k = 0; k < npts; ++k) {
            float lon = 0.0f, lat = 0.0f;
            bf.read(reinterpret_cast<char*>(&lon), sizeof(lon));
            bf.read(reinterpret_cast<char*>(&lat), sizeof(lat));
            /* worldAt() renvoie des coordonnées MONDE ; le pavage pixel ci-dessous
               attend des coordonnées LOCALES (centrées sur 0) : conversion nécessaire
               sur une carte recadrée (originX/originZ non nuls), sans quoi le masque
               de bâtiments tombe à côté et des arbres poussent sur les toits. */
            float lx = 0.0f, lz = 0.0f;
            terrain.worldAt(lon, lat, lx, lz);
            lx -= terrain.originX();
            lz -= terrain.originZ();
            const float fx = (lx + halfW) / (2.0f * halfW) * static_cast<float>(orthoW - 1);
            const float fy = (lz + halfH) / (2.0f * halfH) * static_cast<float>(orthoH - 1);
            bx.push_back(fx);
            by.push_back(fy);
            cmin = std::min(cmin, fx);
            cmax = std::max(cmax, fx);
            rmin = std::min(rmin, fy);
            rmax = std::max(rmax, fy);
        }
        if (!bf) {
            break;
        }
        /* Rastérisation : pixels du cadre englobant qui tombent dans le polygone
           (lancer de rayon), plus une marge d'un pixel autour. */
        const int x0 = std::max(0, static_cast<int>(std::floor(cmin)) - 1);
        const int x1 = std::min(orthoW - 1, static_cast<int>(std::ceil(cmax)) + 1);
        const int y0 = std::max(0, static_cast<int>(std::floor(rmin)) - 1);
        const int y1 = std::min(orthoH - 1, static_cast<int>(std::ceil(rmax)) + 1);
        const std::size_t n = bx.size();
        for (int py = y0; py <= y1; ++py) {
            for (int px = x0; px <= x1; ++px) {
                const float X = static_cast<float>(px) + 0.5f;
                const float Y = static_cast<float>(py) + 0.5f;
                bool in = false;
                for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
                    if (((by[i] > Y) != (by[j] > Y)) &&
                        (X < (bx[j] - bx[i]) * (Y - by[i]) / (by[j] - by[i]) + bx[i])) {
                        in = !in;
                    }
                }
                if (in) { /* marge : le pixel et ses 4 voisins */
                    building[static_cast<std::size_t>(py) * static_cast<std::size_t>(orthoW) +
                             static_cast<std::size_t>(px)] = 1;
                    if (px > 0)
                        building[static_cast<std::size_t>(py) * static_cast<std::size_t>(orthoW) +
                                 static_cast<std::size_t>(px - 1)] = 1;
                    if (px < orthoW - 1)
                        building[static_cast<std::size_t>(py) * static_cast<std::size_t>(orthoW) +
                                 static_cast<std::size_t>(px + 1)] = 1;
                    if (py > 0)
                        building[static_cast<std::size_t>(py - 1) *
                                     static_cast<std::size_t>(orthoW) +
                                 static_cast<std::size_t>(px)] = 1;
                    if (py < orthoH - 1)
                        building[static_cast<std::size_t>(py + 1) *
                                     static_cast<std::size_t>(orthoW) +
                                 static_cast<std::size_t>(px)] = 1;
                }
            }
        }
    }

    return building;
}

} /* namespace artouste::render */
