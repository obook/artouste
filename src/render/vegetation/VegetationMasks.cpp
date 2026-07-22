/*
 * VegetationMasks.cpp
 * Construction des masques qui excluent des zones du semis de végétation :
 * masque d'eau (remplissage de proche en proche depuis les repères "Lac"),
 * masque d'emprise des bâtiments (buildings.bin rastérisé), et zones
 * d'exclusion (exclusions.txt). Extrait de Vegetation.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/Vegetation.hpp"

#include "render/Terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace artouste::render {

namespace {

/* Borne du remplissage de proche en proche (flood fill) par lac, en pixels
   (anti-emballement sur une orthophoto très grande). */
constexpr std::size_t FLOOD_CAP = 500'000;

/* Signature de couleur de l'eau dans l'orthophoto : sombre (le plus clair des trois
   canaux reste bas) et peu saturée (les trois canaux sont proches). Vaut aussi bien
   pour les lacs sombres (Fabrèges) que pour l'eau verte des lacs d'altitude
   (Pombie). Sert de critère de propagation au remplissage depuis un lac : comme on
   ne part que d'un repère de lac connu, seule l'eau reliée à ce lac est masquée. */
bool looksLikeWater(float r, float g, float b) {
    const float hi = std::max(r, std::max(g, b));
    const float lo = std::min(r, std::min(g, b));
    return hi < 0.38f && (hi - lo) < 0.12f;
}

}  /* namespace */

std::vector<unsigned char> Vegetation::buildWaterMask(
    const std::filesystem::path& terrainDir, const Terrain& terrain, const unsigned char* ortho,
    int orthoW, int orthoH, float halfW, float halfH,
    std::vector<std::pair<float, float>>& fallbackLakes) const {
    auto orthoRGBAt = [&](int ox, int oy, float& r, float& g, float& b) {
        orthoRGB(ortho, orthoW, ox, oy, r, g, b);
    };

    /* Masque d'eau : à partir de chaque repère "Lac" de landmarks.txt, on propage de
       proche en proche (flood fill) sur les pixels d'eau connectés, ce qui épouse la
       forme réelle du plan d'eau (un simple disque ratait les lacs allongés comme le
       réservoir de Fabrèges). Les lacs dont le repère n'est pas sur de l'eau (repère
       mal placé) sont rangés à part, dégagés ensuite par un petit disque de secours. */
    std::vector<unsigned char> water(static_cast<std::size_t>(orthoW)
                                     * static_cast<std::size_t>(orthoH), 0);
    {
        std::ifstream f(terrainDir / "landmarks.txt");
        std::string   line;
        while (std::getline(f, line)) {
            const std::size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos || line[first] == '#') {
                continue;  /* ligne vide ou commentaire */
            }
            float              lon = 0.0f, lat = 0.0f;
            std::istringstream iss(line);
            if (!(iss >> lon >> lat)) {
                continue;
            }
            std::string name;
            std::getline(iss, name);
            const std::size_t nf = name.find_first_not_of(" \t\r\n");
            if (nf == std::string::npos || name.compare(nf, 3, "Lac") != 0) {
                continue;  /* pas un lac */
            }
            /* worldAt() renvoie des coordonnées MONDE ; toPixel() attend des
               coordonnées LOCALES (centrées sur 0, comme halfW/halfH) : conversion
               nécessaire sur une carte recadrée (originX/originZ non nuls). */
            float lx = 0.0f, lz = 0.0f;
            terrain.worldAt(lon, lat, lx, lz);
            lx -= terrain.originX();
            lz -= terrain.originZ();
            int cx = 0, cy = 0;
            toPixel(lx, lz, halfW, halfH, orthoW, orthoH, cx, cy);

            /* Graine du remplissage : un pixel d'eau proche du repère (le repère
               peut être posé un peu à côté du plan d'eau). */
            int seedX = -1, seedY = -1;
            for (int dy = -12; dy <= 12 && seedX < 0; ++dy) {
                for (int dx = -12; dx <= 12; ++dx) {
                    const int px = std::min(std::max(cx + dx, 0), orthoW - 1);
                    const int py = std::min(std::max(cy + dy, 0), orthoH - 1);
                    float r = 0.0f, g = 0.0f, b = 0.0f;
                    orthoRGBAt(px, py, r, g, b);
                    if (looksLikeWater(r, g, b)) {
                        seedX = px;
                        seedY = py;
                        break;
                    }
                }
            }
            if (seedX < 0) {
                fallbackLakes.emplace_back(lx, lz);  /* pas d'eau sous le repère */
                continue;
            }

            /* Propagation 4-connexe sur les pixels d'eau. */
            std::vector<int> stack;
            stack.push_back(seedY * orthoW + seedX);
            water[static_cast<std::size_t>(seedY) * static_cast<std::size_t>(orthoW)
                  + static_cast<std::size_t>(seedX)] = 1;
            std::size_t filled = 0;
            while (!stack.empty() && filled < FLOOD_CAP) {
                const int idx = stack.back();
                stack.pop_back();
                ++filled;
                const int px = idx % orthoW;
                const int py = idx / orthoW;
                const int nb[4][2] = {{px - 1, py}, {px + 1, py}, {px, py - 1}, {px, py + 1}};
                for (const auto& n : nb) {
                    if (n[0] < 0 || n[0] >= orthoW || n[1] < 0 || n[1] >= orthoH) {
                        continue;
                    }
                    const std::size_t ni = static_cast<std::size_t>(n[1]) * static_cast<std::size_t>(orthoW)
                                           + static_cast<std::size_t>(n[0]);
                    if (water[ni] != 0) {
                        continue;
                    }
                    float r = 0.0f, g = 0.0f, b = 0.0f;
                    orthoRGBAt(n[0], n[1], r, g, b);
                    if (looksLikeWater(r, g, b)) {
                        water[ni] = 1;
                        stack.push_back(n[1] * orthoW + n[0]);
                    }
                }
            }
        }
    }

    /* Marge autour des lacs : on dilate le masque d'eau de quelques pixels, pour
       dégager le CONTOUR (grève, gravier de la rive), dont la couleur pâle-verte est
       trop proche de la forêt pour être séparée à la seule couleur. Dilatation en
       disque à partir des pixels d'eau (peu nombreux), sur une copie de l'état non
       dilaté pour ne pas propager en chaîne. */
    {
        const std::vector<unsigned char> orig = water;
        constexpr int                    R  = 5;  /* ~22 m de marge (ortho ~4,4 m/pixel) */
        const int                        r2 = R * R;
        for (int y = 0; y < orthoH; ++y) {
            for (int x = 0; x < orthoW; ++x) {
                if (orig[static_cast<std::size_t>(y) * static_cast<std::size_t>(orthoW)
                         + static_cast<std::size_t>(x)] == 0) {
                    continue;
                }
                for (int dy = -R; dy <= R; ++dy) {
                    const int yy = y + dy;
                    if (yy < 0 || yy >= orthoH) {
                        continue;
                    }
                    for (int dx = -R; dx <= R; ++dx) {
                        const int xx = x + dx;
                        if (xx < 0 || xx >= orthoW || dx * dx + dy * dy > r2) {
                            continue;
                        }
                        water[static_cast<std::size_t>(yy) * static_cast<std::size_t>(orthoW)
                              + static_cast<std::size_t>(xx)] = 1;
                    }
                }
            }
        }
    }

    return water;
}

std::vector<unsigned char> Vegetation::buildBuildingMask(const std::filesystem::path& terrainDir,
                                                          const Terrain& terrain, int orthoW,
                                                          int orthoH, float halfW,
                                                          float halfH) const {
    /* Masque des bâtiments : on ne plante pas d'arbre sur une emprise de bâtiment
       (sinon des arbres poussent sur les toits dans les villages, là où l'ortho est
       verte entre les maisons). On lit buildings.bin (mêmes emprises que render::
       Buildings), on rastérise chaque polygone dans un masque à la résolution de
       l'ortho, avec un pixel de marge pour que le billboard ne mange pas les murs.
       Format : magie "ABLD", version, nombre, puis par bâtiment hauteur(float),
       n(uint16), n x (lon,lat float). */
    std::vector<unsigned char> building(static_cast<std::size_t>(orthoW)
                                        * static_cast<std::size_t>(orthoH), 0);
    std::ifstream bf(terrainDir / "buildings.bin", std::ios::binary);
    char          magic[4] = {0, 0, 0, 0};
    std::uint32_t ver = 0, count = 0;
    if (bf) {
        bf.read(magic, 4);
        bf.read(reinterpret_cast<char*>(&ver), sizeof(ver));
        bf.read(reinterpret_cast<char*>(&count), sizeof(count));
    }
    const bool ok = bf && magic[0] == 'A' && magic[1] == 'B' && magic[2] == 'L'
                    && magic[3] == 'D' && ver == 1u;
    std::vector<float> bx, by;  /* emprise en coordonnées pixel (réutilisé par bâtiment) */
    for (std::uint32_t bi = 0; ok && bi < count; ++bi) {
        float         height = 0.0f;
        std::uint16_t npts   = 0;
        bf.read(reinterpret_cast<char*>(&height), sizeof(height));
        bf.read(reinterpret_cast<char*>(&npts), sizeof(npts));
        if (!bf || npts < 3) {
            break;  /* fichier tronqué ou emprise dégénérée */
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
                bool        in = false;
                for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
                    if (((by[i] > Y) != (by[j] > Y))
                        && (X < (bx[j] - bx[i]) * (Y - by[i]) / (by[j] - by[i]) + bx[i])) {
                        in = !in;
                    }
                }
                if (in) {  /* marge : le pixel et ses 4 voisins */
                    building[static_cast<std::size_t>(py) * static_cast<std::size_t>(orthoW)
                             + static_cast<std::size_t>(px)] = 1;
                    if (px > 0) building[static_cast<std::size_t>(py) * static_cast<std::size_t>(orthoW) + static_cast<std::size_t>(px - 1)] = 1;
                    if (px < orthoW - 1) building[static_cast<std::size_t>(py) * static_cast<std::size_t>(orthoW) + static_cast<std::size_t>(px + 1)] = 1;
                    if (py > 0) building[static_cast<std::size_t>(py - 1) * static_cast<std::size_t>(orthoW) + static_cast<std::size_t>(px)] = 1;
                    if (py < orthoH - 1) building[static_cast<std::size_t>(py + 1) * static_cast<std::size_t>(orthoW) + static_cast<std::size_t>(px)] = 1;
                }
            }
        }
    }

    return building;
}

std::vector<Vegetation::Exclusion> Vegetation::loadExclusions(
    const std::filesystem::path& terrainDir, const Terrain& terrain) const {
    /* Zones d'exclusion (aérodromes, etc.) : cercles "lon lat rayon_m" lus depuis
       exclusions.txt, où l'on ne plante aucun arbre. L'orthophoto y est verte
       (pistes et bandes enherbées) et passerait le test de forêt : on les écarte
       explicitement. Chaque cercle est converti une fois en coordonnées monde
       (centre + rayon au carré), comme le dégagement de départ et les lacs de
       secours. Fichier optionnel : absent, aucune exclusion. */
    std::vector<Exclusion> exclusions;
    std::ifstream f(terrainDir / "exclusions.txt");
    std::string   line;
    while (std::getline(f, line)) {
        const std::size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#') {
            continue;  /* ligne vide ou commentaire */
        }
        float              lon = 0.0f, lat = 0.0f, radius = 0.0f;
        std::istringstream iss(line);
        if (!(iss >> lon >> lat >> radius) || radius <= 0.0f) {
            continue;
        }
        /* worldAt() renvoie des coordonnées MONDE ; le semis (VegetationScatter.cpp)
           compare ces cercles à des coordonnées LOCALES : conversion nécessaire sur
           une carte recadrée (originX/originZ non nuls), sinon l'exclusion (piste
           d'aérodrome, etc.) tombe à côté et n'écarte pas les bons arbres. */
        float ex = 0.0f, ez = 0.0f;
        terrain.worldAt(lon, lat, ex, ez);
        ex -= terrain.originX();
        ez -= terrain.originZ();
        exclusions.push_back({ex, ez, radius * radius});
    }
    return exclusions;
}

}  /* namespace artouste::render */
