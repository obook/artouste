/*
 * Buildings.cpp
 * Lit buildings.bin (emprises BD TOPO + hauteur) et extrude chaque bâtiment en
 * un volume simple : murs verticaux (un quad par côté) et toit plat. Tout est
 * fusionné en un maillage unique, calé sur le relief.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Buildings.hpp"

#include "render/Terrain.hpp"
#include "util/Math.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <utility>
#include <vector>

namespace artouste::render {

namespace {

/* Couleurs régionales (côte basque et Landes) : murs clairs, toitures en tuile.
   Une légère variation par bâtiment évite l'aspect uniforme d'une ville monochrome. */
const vec3 WALL_COLOR{0.86f, 0.84f, 0.80f};  /* enduit clair */
const vec3 ROOF_COLOR{0.62f, 0.32f, 0.24f};  /* tuile terre cuite */

/* Bâtiments au ras de l'eau : la BD TOPO inclut les cabanes ostréicoles bâties sur
   le bassin (ports de Gujan-Mestras, La Teste, Cap Ferret). Posées sur un sol à
   l'altitude de l'eau, elles "flottent" sur la mer rendue. On les écarte en repérant
   les bâtiments dont le sol est bas ET de couleur d'eau dans l'orthophoto. La seule
   altitude ne suffit pas : la carte de relief est grossière (~70 m/pixel) et rate les
   chenaux étroits des ports. Dans l'ortho, l'eau a un rouge faible et un bleu nettement
   supérieur au rouge (b - r grand), ce qui la distingue des ombres et toits sombres
   (gris neutres) comme des terres habitées (claires). Les trois conditions réunies
   ciblent l'eau sans toucher les villes côtières basses.
   WATER_ALT_M : un bâtiment plus haut que ça n'est jamais sur l'eau (ville en hauteur).
   WATER_RED / WATER_BLUE_BIAS : signature couleur de l'eau (canaux normalisés 0..1). */
constexpr float WATER_ALT_M      = 4.0f;
constexpr float WATER_RED        = 0.30f;
constexpr float WATER_BLUE_BIAS  = 0.08f;

/* Petit générateur pseudo-aléatoire déterministe (sans état global) : à partir
   d'un entier, renvoie un facteur dans [1-amp, 1+amp] pour nuancer une couleur. */
float jitter(std::uint32_t seed, float amp) {
    seed = seed * 1664525u + 1013904223u;            /* LCG classique */
    const float unit = static_cast<float>(seed >> 8) / 16777216.0f;  /* [0,1) */
    return 1.0f + (unit * 2.0f - 1.0f) * amp;
}

void pushTriangle(std::vector<Vertex>& verts, std::vector<unsigned int>& idx,
                  const vec3& a, const vec3& b, const vec3& c, const vec3& normal,
                  const vec3& color) {
    const auto base = static_cast<unsigned int>(verts.size());
    verts.push_back(Vertex{a, normal, color, {0.0f, 0.0f}});
    verts.push_back(Vertex{b, normal, color, {0.0f, 0.0f}});
    verts.push_back(Vertex{c, normal, color, {0.0f, 0.0f}});
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
}

}  /* namespace */

Buildings::Buildings(const std::filesystem::path& dir, const Terrain& terrain) {
    const std::filesystem::path path = dir / "buildings.bin";
    std::ifstream               in(path, std::ios::binary);
    if (!in) {
        return;  /* pas de bâtiments pour ce terrain : maillage vide */
    }

    char magic[4] = {0, 0, 0, 0};
    in.read(magic, 4);
    std::uint32_t version = 0, count = 0;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in || magic[0] != 'A' || magic[1] != 'B' || magic[2] != 'L' || magic[3] != 'D'
        || version != 1u) {
        std::fprintf(stderr, "[Buildings] %s : en-tête invalide, bâtiments ignorés.\n",
                     path.string().c_str());
        return;
    }

    /* Centres des hélipads (départ + ceux du terrain), en coordonnées monde : on
       n'extrude aucun bâtiment trop proche, pour ne pas en poser un sur un pad (ce
       qui le masquerait et provoquerait du z-fighting). */
    std::vector<std::pair<float, float>> padCenters;
    if (terrain.hasStart()) {
        padCenters.emplace_back(terrain.startX(), terrain.startZ());
    }
    for (const Landmark& pad : terrain.helipads()) {
        float px = 0.0f, pz = 0.0f;
        terrain.worldAt(pad.lon, pad.lat, px, pz);
        padCenters.emplace_back(px, pz);
    }
    constexpr float PAD_CLEAR_M2 = 15.0f * 15.0f;  /* rayon dégagé autour d'un pad, au carré */

    std::vector<Vertex>       verts;
    std::vector<unsigned int> idx;
    /* Estimation grossière (murs + toit, ~5 côtés en moyenne) pour limiter les
       réallocations sur les grandes villes. */
    verts.reserve(static_cast<std::size_t>(count) * 25);
    idx.reserve(static_cast<std::size_t>(count) * 45);

    /* Le filtre "bâtiment sur l'eau" ne vaut que pour les terrains de bord de mer
       (drapeau sea). En montagne (sea 0), le niveau 0 est juste le point le plus bas
       du relief : on y garde les cabanes et refuges, même au bord d'un lac d'altitude. */
    const bool filterWater = terrain.drawsSea();

    /* Orthophoto chargée côté CPU (uniquement pour le filtre eau) : on y lit la
       couleur du sol sous chaque bâtiment. Demi-dimensions du terrain pour convertir
       une position monde en coordonnées de pixel (origine au centre du bloc). */
    int            orthoW = 0, orthoH = 0, orthoCh = 0;
    unsigned char* ortho  = nullptr;
    if (filterWater) {
        const std::filesystem::path orthoPath = dir / "ortho.jpg";
        stbi_set_flip_vertically_on_load(0);  /* rangée 0 = nord, comme le relief */
        ortho = stbi_load(orthoPath.string().c_str(), &orthoW, &orthoH, &orthoCh, 3);
    }
    const float halfW = terrain.halfWidth();
    const float halfH = terrain.halfHeight();

    std::vector<float> px, pz;  /* emprise en coordonnées monde (réutilisé par bâtiment) */
    std::size_t        skippedWater = 0;  /* bâtiments écartés car bâtis sur l'eau */
    for (std::uint32_t b = 0; b < count; ++b) {
        float         height = 0.0f;
        std::uint16_t npts   = 0;
        in.read(reinterpret_cast<char*>(&height), sizeof(height));
        in.read(reinterpret_cast<char*>(&npts), sizeof(npts));
        if (!in || npts < 3) {
            break;  /* fichier tronqué ou bâtiment dégénéré : on s'arrête */
        }

        px.clear();
        pz.clear();
        float base   = 1e9f;   /* altitude du sol la plus basse sous l'emprise */
        float cx     = 0.0f;
        float cz     = 0.0f;
        for (std::uint16_t k = 0; k < npts; ++k) {
            float lon = 0.0f, lat = 0.0f;
            in.read(reinterpret_cast<char*>(&lon), sizeof(lon));
            in.read(reinterpret_cast<char*>(&lat), sizeof(lat));
            float x = 0.0f, z = 0.0f;
            terrain.worldAt(lon, lat, x, z);
            px.push_back(x);
            pz.push_back(z);
            base = std::min(base, terrain.heightAt(x, z));
            cx += x;
            cz += z;
        }
        if (!in) {
            break;
        }
        cx /= static_cast<float>(npts);
        cz /= static_cast<float>(npts);

        /* Cabane bâtie sur l'eau (port ostréicole) : sol bas ET de couleur d'eau dans
           l'ortho (voir WATER_ALT_M / WATER_RED / WATER_BLUE_BIAS). On la teste au
           centre de l'emprise et on l'écarte pour ne pas la faire flotter sur le bassin. */
        if (filterWater && ortho != nullptr && base <= WATER_ALT_M) {
            const float u  = (cx + halfW) / (2.0f * halfW);   /* 0 = ouest, 1 = est */
            const float v  = (cz + halfH) / (2.0f * halfH);   /* 0 = nord,  1 = sud */
            const int   ox = std::clamp(static_cast<int>(u * (orthoW - 1)), 0, orthoW - 1);
            const int   oy = std::clamp(static_cast<int>(v * (orthoH - 1)), 0, orthoH - 1);
            const unsigned char* p = ortho + (static_cast<std::size_t>(oy) * orthoW + ox) * 3;
            const float r = p[0] / 255.0f;
            const float bl = p[2] / 255.0f;
            if (r <= WATER_RED && (bl - r) >= WATER_BLUE_BIAS) {
                ++skippedWater;
                continue;
            }
        }

        /* On laisse les hélipads dégagés. Un bâtiment est ignoré s'il gêne un pad,
           dans trois cas : son emprise CONTIENT le pad (gros bâtiment qui le
           recouvre, comme l'hôpital), un de ses sommets est tout près du pad, ou son
           centre est près du pad. Le test point-dans-polygone (lancer de rayon)
           attrape le cas du grand bâtiment, que la seule distance au centre ratait. */
        const std::size_t n = px.size();
        const auto inFootprint = [&](float X, float Z) {
            bool in = false;
            for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
                if (((pz[i] > Z) != (pz[j] > Z))
                    && (X < (px[j] - px[i]) * (Z - pz[i]) / (pz[j] - pz[i]) + px[i])) {
                    in = !in;
                }
            }
            return in;
        };
        bool onPad = false;
        for (const auto& p : padCenters) {
            if (inFootprint(p.first, p.second)) {
                onPad = true;
                break;
            }
            const float dcx = cx - p.first, dcz = cz - p.second;
            if (dcx * dcx + dcz * dcz < PAD_CLEAR_M2) {
                onPad = true;
                break;
            }
            for (std::size_t k = 0; k < n; ++k) {
                const float ddx = px[k] - p.first, ddz = pz[k] - p.second;
                if (ddx * ddx + ddz * ddz < PAD_CLEAR_M2) {
                    onPad = true;
                    break;
                }
            }
            if (onPad) {
                break;
            }
        }
        if (onPad) {
            continue;
        }

        const float top = base + height;

        const float rj   = jitter(b, 0.12f);
        const vec3  wall = WALL_COLOR;
        const vec3  roof = ROOF_COLOR * rj;

        /* Murs : un quad vertical par côté de l'emprise. */
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j  = (i + 1) % n;
            const float       ex = px[j] - px[i];
            const float       ez = pz[j] - pz[i];
            /* Normale horizontale perpendiculaire au côté, orientée vers
               l'extérieur (à l'opposé du centre de l'emprise). */
            float nx = ez, nz = -ex;
            const float mx = 0.5f * (px[i] + px[j]) - cx;
            const float mz = 0.5f * (pz[i] + pz[j]) - cz;
            if (nx * mx + nz * mz < 0.0f) {
                nx = -nx;
                nz = -nz;
            }
            const vec3 normal = glm::normalize(vec3{nx, 0.0f, nz});
            const vec3 bi{px[i], base, pz[i]};
            const vec3 bj{px[j], base, pz[j]};
            const vec3 tj{px[j], top, pz[j]};
            const vec3 ti{px[i], top, pz[i]};
            pushTriangle(verts, idx, bi, bj, tj, normal, wall);
            pushTriangle(verts, idx, bi, tj, ti, normal, wall);
        }

        /* Toit plat : éventail de triangles depuis le premier sommet (correct pour
           une emprise convexe, acceptable de loin pour les rares formes concaves). */
        const vec3 up{0.0f, 1.0f, 0.0f};
        for (std::size_t i = 1; i + 1 < n; ++i) {
            const vec3 a{px[0], top, pz[0]};
            const vec3 c{px[i], top, pz[i]};
            const vec3 d{px[i + 1], top, pz[i + 1]};
            pushTriangle(verts, idx, a, c, d, up, roof);
        }

        ++m_count;
    }

    if (ortho != nullptr) {
        stbi_image_free(ortho);
    }
    if (skippedWater > 0) {
        std::printf("[Buildings] %zu bâtiment(s) sur l'eau écarté(s) (cabanes au ras du bassin).\n",
                    skippedWater);
    }
    if (m_count == 0) {
        return;
    }
    m_mesh = Mesh(verts, idx);
    std::printf("[Buildings] %zu bâtiments extrudés (%zu sommets).\n", m_count, verts.size());
}

}  /* namespace artouste::render */
