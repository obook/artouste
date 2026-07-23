/*
 * VegetationWaterMask.cpp
 * Masque d'eau qui exclut les lacs du semis de végétation : remplissage de
 * proche en proche (flood fill) depuis les repères "Lac" de landmarks.txt,
 * puis dilatation pour dégager la rive. Le masque des bâtiments est dans
 * VegetationBuildingMask.cpp, les zones d'exclusion dans VegetationMasks.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"
#include "render/Vegetation.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

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

} /* namespace */

std::vector<unsigned char>
Vegetation::buildWaterMask(const std::filesystem::path& terrainDir,
                           const Terrain& terrain,
                           const unsigned char* ortho,
                           int orthoW,
                           int orthoH,
                           float halfW,
                           float halfH,
                           std::vector<std::pair<float, float>>& fallbackLakes) const {
    auto orthoRGBAt = [&](int ox, int oy, float& r, float& g, float& b) {
        orthoRGB(ortho, orthoW, ox, oy, r, g, b);
    };

    /* Masque d'eau : à partir de chaque repère "Lac" de landmarks.txt, on propage de
       proche en proche (flood fill) sur les pixels d'eau connectés, ce qui épouse la
       forme réelle du plan d'eau (un simple disque ratait les lacs allongés comme le
       réservoir de Fabrèges). Les lacs dont le repère n'est pas sur de l'eau (repère
       mal placé) sont rangés à part, dégagés ensuite par un petit disque de secours. */
    std::vector<unsigned char> water(
        static_cast<std::size_t>(orthoW) * static_cast<std::size_t>(orthoH), 0);
    {
        std::ifstream f(terrainDir / "landmarks.txt");
        std::string line;
        while (std::getline(f, line)) {
            const std::size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos || line[first] == '#') {
                continue; /* ligne vide ou commentaire */
            }
            float lon = 0.0f, lat = 0.0f;
            std::istringstream iss(line);
            if (!(iss >> lon >> lat)) {
                continue;
            }
            std::string name;
            std::getline(iss, name);
            const std::size_t nf = name.find_first_not_of(" \t\r\n");
            if (nf == std::string::npos || name.compare(nf, 3, "Lac") != 0) {
                continue; /* pas un lac */
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
                fallbackLakes.emplace_back(lx, lz); /* pas d'eau sous le repère */
                continue;
            }

            /* Propagation 4-connexe sur les pixels d'eau. */
            std::vector<int> stack;
            stack.push_back(seedY * orthoW + seedX);
            water[static_cast<std::size_t>(seedY) * static_cast<std::size_t>(orthoW) +
                  static_cast<std::size_t>(seedX)] = 1;
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
                    const std::size_t ni =
                        static_cast<std::size_t>(n[1]) * static_cast<std::size_t>(orthoW) +
                        static_cast<std::size_t>(n[0]);
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
        constexpr int R = 5; /* ~22 m de marge (ortho ~4,4 m/pixel) */
        const int r2 = R * R;
        for (int y = 0; y < orthoH; ++y) {
            for (int x = 0; x < orthoW; ++x) {
                if (orig[static_cast<std::size_t>(y) * static_cast<std::size_t>(orthoW) +
                         static_cast<std::size_t>(x)] == 0) {
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
                        water[static_cast<std::size_t>(yy) * static_cast<std::size_t>(orthoW) +
                              static_cast<std::size_t>(xx)] = 1;
                    }
                }
            }
        }
    }

    return water;
}

} /* namespace artouste::render */
