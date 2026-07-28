/*
 * BuildingsMesh.cpp
 * Lit buildings.bin (emprises BD TOPO + hauteur) et extrude chaque bâtiment en
 * un volume simple : murs verticaux (un quad par côté) et toit plat. Tout est
 * fusionné en un maillage unique, calé sur le relief, découpé en tuiles
 * spatiales pour le culling. Le culling et le dessin sont dans
 * BuildingsDraw.cpp.
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
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <utility>
#include <vector>

namespace artouste::render {

namespace {

/* Couleurs régionales (côte basque et Landes) : murs clairs, toitures en tuile.
   Une légère variation par bâtiment évite l'aspect uniforme d'une ville monochrome. */
const vec3 WALL_COLOR{0.86f, 0.84f, 0.80f}; /* enduit clair */

/* Taille réelle (mètres) de la tuile de façade (assets/textures/facade.png,
   voir tools/facade/generer_facade.py -- mêmes valeurs des deux côtés) : les UV
   des murs sont calculés en mètres réels / cette taille, pour que la texture se
   pose à la même échelle sur un pavillon et sur un immeuble, sans dépendre du
   nombre de sommets de l'emprise. */
constexpr float FACADE_TILE_W_M = 12.0f;
constexpr float FACADE_TILE_H_M = 6.0f;

/* Palette de toitures : plutôt que la même tuile partout, on panache quelques
   teintes pour donner une impression de variété (toits neufs, patinés, quelques
   ardoises). La tuile terre cuite reste dominante : elle est répétée pour peser
   plus lourd dans le tirage (3 entrées sur 6, soit ~50 %). Une teinte est choisie
   par bâtiment, de façon stable (voir pickRoof). */
const vec3 ROOF_PALETTE[] = {
    {0.62f, 0.32f, 0.24f}, /* tuile terre cuite (dominante) */
    {0.62f, 0.32f, 0.24f}, /*   -- pesée 3 fois sur 6      */
    {0.62f, 0.32f, 0.24f},
    {0.70f, 0.38f, 0.27f}, /* tuile plus chaude / neuve */
    {0.48f, 0.26f, 0.21f}, /* tuile patinée, plus sombre */
    {0.44f, 0.42f, 0.44f}, /* ardoise grise */
};

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
constexpr float WATER_ALT_M = 4.0f;
constexpr float WATER_RED = 0.30f;
constexpr float WATER_BLUE_BIAS = 0.08f;

/* Petit générateur pseudo-aléatoire déterministe (sans état global) : à partir
   d'un entier, renvoie un facteur dans [1-amp, 1+amp] pour nuancer une couleur. */
float jitter(std::uint32_t seed, float amp) {
    seed = seed * 1664525u + 1013904223u;                           /* LCG classique */
    const float unit = static_cast<float>(seed >> 8) / 16777216.0f; /* [0,1) */
    return 1.0f + (unit * 2.0f - 1.0f) * amp;
}

/* Choisit une teinte de toiture dans ROOF_PALETTE pour un bâtiment donné. Le choix
   est stable (fonction de son seul indice : pas de scintillement d'une image à
   l'autre) et décorrélé du jitter de luminosité (on brasse l'indice par un autre
   hash), pour que teinte et nuance ne varient pas de concert. */
const vec3& pickRoof(std::uint32_t seed) {
    seed ^= 0x9e3779b9u;                     /* décorrèle du hash de jitter */
    seed = seed * 2654435761u + 2246822519u; /* mélange */
    const std::size_t n = sizeof(ROOF_PALETTE) / sizeof(ROOF_PALETTE[0]);
    return ROOF_PALETTE[(seed >> 16) % n];
}

void pushTriangle(std::vector<Vertex>& verts,
                  std::vector<unsigned int>& idx,
                  const vec3& a,
                  const vec3& b,
                  const vec3& c,
                  const vec3& normal,
                  const vec3& color) {
    const auto base = static_cast<unsigned int>(verts.size());
    verts.push_back(Vertex{a, normal, color, {0.0f, 0.0f}});
    verts.push_back(Vertex{b, normal, color, {0.0f, 0.0f}});
    verts.push_back(Vertex{c, normal, color, {0.0f, 0.0f}});
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
}

/* Quad d'un mur (bi, bj en bas ; ti, tj en haut, mêmes côtés), avec des UV
   réels : u parcourt la largeur du côté (0 à sa longueur / FACADE_TILE_W_M), v
   la hauteur depuis le sol du bâtiment (0 à sa hauteur / FACADE_TILE_H_M). La
   texture de façade (voir building.frag) se répète ainsi à échelle constante
   quelle que soit la taille du bâtiment. */
void pushWallQuad(std::vector<Vertex>& verts,
                  std::vector<unsigned int>& idx,
                  const vec3& bi,
                  const vec3& bj,
                  const vec3& tj,
                  const vec3& ti,
                  const vec3& normal,
                  const vec3& color,
                  float u1,
                  float vTop) {
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

} /* namespace */

Buildings::Buildings(const std::filesystem::path& dir, const Terrain& terrain) {
    const std::filesystem::path path = dir / "buildings.bin";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return; /* pas de bâtiments pour ce terrain : maillage vide */
    }

    char magic[4] = {0, 0, 0, 0};
    in.read(magic, 4);
    std::uint32_t version = 0, count = 0;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in || magic[0] != 'A' || magic[1] != 'B' || magic[2] != 'L' || magic[3] != 'D' ||
        version != 1u) {
        std::fprintf(stderr,
                     "[Buildings] %s : en-tête invalide, bâtiments ignorés.\n",
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
    constexpr float PAD_CLEAR_M2 = 15.0f * 15.0f; /* rayon dégagé autour d'un pad, au carré */

    /* Monuments 3D de la carte : leur modèle décrit déjà l'édifice, et la BD TOPO
       en donne l'emprise extrudée. Poser les deux imbriquerait deux géométries
       (la tour Eiffel deviendrait un bloc à fenêtres traversé par un treillis).
       On dégage donc, autour de chaque monument, le rayon qu'il déclare dans
       monuments.txt. Centres convertis une fois pour toutes en monde. */
    struct MonumentClear {
        float x = 0.0f;
        float z = 0.0f;
        float radius2 = 0.0f;
    };
    std::vector<MonumentClear> monumentClears;
    for (const Monument& mon : terrain.monuments()) {
        if (mon.clearRadiusM <= 0.0f) {
            continue; /* monument sans dégagement demandé : les bâtiments restent */
        }
        MonumentClear mc;
        terrain.worldAt(mon.lon, mon.lat, mc.x, mc.z);
        mc.radius2 = mon.clearRadiusM * mon.clearRadiusM;
        monumentClears.push_back(mc);
    }

    /* La géométrie n'est plus accumulée dans un tampon unique : chaque bâtiment est
       rangé dans une tuile spatiale (voir plus bas), puis les tuiles sont concaténées
       en fin de construction. Cela permet le culling par tuile au rendu. */

    /* Le filtre "bâtiment sur l'eau" ne vaut que pour les terrains de bord de mer
       (drapeau sea). En montagne (sea 0), le niveau 0 est juste le point le plus bas
       du relief : on y garde les cabanes et refuges, même au bord d'un lac d'altitude. */
    const bool filterWater = terrain.drawsSea();

    /* Orthophoto chargée côté CPU (uniquement pour le filtre eau) : on y lit la
       couleur du sol sous chaque bâtiment. Demi-dimensions du terrain pour convertir
       une position monde en coordonnées de pixel (origine au centre du bloc). */
    int orthoW = 0, orthoH = 0, orthoCh = 0;
    unsigned char* ortho = nullptr;
    if (filterWater) {
        const std::filesystem::path orthoPath = dir / "ortho.jpg";
        stbi_set_flip_vertically_on_load(0); /* rangée 0 = nord, comme le relief */
        ortho = stbi_load(orthoPath.string().c_str(), &orthoW, &orthoH, &orthoCh, 3);
    }
    const float halfW = terrain.halfWidth();
    const float halfH = terrain.halfHeight();
    const float origX = terrain.originX(); /* centre de l'emprise (0 sauf carte recadrée) */
    const float origZ = terrain.originZ();

    /* Grille de tuiles pour le culling au rendu : chaque bâtiment est rangé dans la
       tuile de son centre. TILE_M fixe la finesse : trop petit multiplie les tuiles
       (surcoût CPU par image), trop grand rend le culling grossier. ~1200 m est un bon
       compromis à l'échelle d'une ville. Chaque tuile accumule sa propre géométrie et
       sa boîte englobante (monde) ; le tout est concaténé après la boucle. */
    constexpr float TILE_M = 2500.0f;
    const int cols = std::max(1, static_cast<int>((2.0f * halfW) / TILE_M) + 1);
    const int rows = std::max(1, static_cast<int>((2.0f * halfH) / TILE_M) + 1);
    struct TileBuild {
        std::vector<Vertex> v;
        std::vector<unsigned int> i;
        vec3 mn{1e30f, 1e30f, 1e30f};
        vec3 mx{-1e30f, -1e30f, -1e30f};
    };
    std::vector<TileBuild> tiles(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));

    std::vector<float> px, pz;    /* emprise en coordonnées monde (réutilisé par bâtiment) */
    std::size_t skippedWater = 0;     /* bâtiments écartés car bâtis sur l'eau */
    std::size_t skippedMonument = 0;  /* bâtiments écartés car sous un monument 3D */
    for (std::uint32_t b = 0; b < count; ++b) {
        float height = 0.0f;
        std::uint16_t npts = 0;
        in.read(reinterpret_cast<char*>(&height), sizeof(height));
        in.read(reinterpret_cast<char*>(&npts), sizeof(npts));
        if (!in || npts < 3) {
            break; /* fichier tronqué ou bâtiment dégénéré : on s'arrête */
        }

        px.clear();
        pz.clear();
        float base = 1e9f; /* altitude du sol la plus basse sous l'emprise */
        float cx = 0.0f;
        float cz = 0.0f;
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

        /* Bâtiment hors de l'emprise courante (carte recadrée) : ignoré, sinon il
           flotterait au-delà du bord du relief. */
        if (std::fabs(cx - origX) > halfW || std::fabs(cz - origZ) > halfH) {
            continue;
        }

        /* Cabane bâtie sur l'eau (port ostréicole) : sol bas ET de couleur d'eau dans
           l'ortho (voir WATER_ALT_M / WATER_RED / WATER_BLUE_BIAS). On la teste au
           centre de l'emprise et on l'écarte pour ne pas la faire flotter sur le bassin. */
        if (filterWater && ortho != nullptr && base <= WATER_ALT_M) {
            const float u = (cx - origX + halfW) / (2.0f * halfW); /* 0 = ouest, 1 = est */
            const float v = (cz - origZ + halfH) / (2.0f * halfH); /* 0 = nord,  1 = sud */
            const int ox =
                std::clamp(static_cast<int>(u * static_cast<float>(orthoW - 1)), 0, orthoW - 1);
            const int oy =
                std::clamp(static_cast<int>(v * static_cast<float>(orthoH - 1)), 0, orthoH - 1);
            const unsigned char* p =
                ortho + (static_cast<std::size_t>(oy) * static_cast<std::size_t>(orthoW) +
                         static_cast<std::size_t>(ox)) *
                            3;
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
            bool inside = false;
            for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
                if (((pz[i] > Z) != (pz[j] > Z)) &&
                    (X < (px[j] - px[i]) * (Z - pz[i]) / (pz[j] - pz[i]) + px[i])) {
                    inside = !inside;
                }
            }
            return inside;
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

        /* Emprise sous un monument 3D : même logique que pour les pads, mais au
           rayon déclaré par le monument. Le test point-dans-polygone rattrape le
           cas de la grande emprise qui contient le monument sans que son centre
           en soit proche (l'esplanade du Champ de Mars autour de la tour). */
        bool underMonument = false;
        for (const MonumentClear& mc : monumentClears) {
            const float dmx = cx - mc.x, dmz = cz - mc.z;
            if (dmx * dmx + dmz * dmz < mc.radius2 || inFootprint(mc.x, mc.z)) {
                underMonument = true;
                break;
            }
        }
        if (underMonument) {
            ++skippedMonument;
            continue;
        }

        const float top = base + height;

        /* Tuile de ce bâtiment (d'après son centre) et mise à jour de sa boîte
           englobante monde (emprise au sol, du sol au sommet). */
        const int col = std::clamp(static_cast<int>((cx - origX + halfW) / TILE_M), 0, cols - 1);
        const int row = std::clamp(static_cast<int>((cz - origZ + halfH) / TILE_M), 0, rows - 1);
        TileBuild& tb = tiles[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
                              static_cast<std::size_t>(col)];
        for (std::size_t k = 0; k < n; ++k) {
            tb.mn.x = std::min(tb.mn.x, px[k]);
            tb.mx.x = std::max(tb.mx.x, px[k]);
            tb.mn.z = std::min(tb.mn.z, pz[k]);
            tb.mx.z = std::max(tb.mx.z, pz[k]);
        }
        tb.mn.y = std::min(tb.mn.y, base);
        tb.mx.y = std::max(tb.mx.y, top);

        const float rj = jitter(b, 0.12f);
        /* Seed décorrélée de celle du toit (voir pickRoof) : mur et toit ne doivent
           pas varier de concert. */
        const float wj = jitter(b * 2654435761u + 1u, 0.08f);
        const vec3 wall = WALL_COLOR * wj;
        const vec3 roof = pickRoof(b) * rj; /* teinte panachée, nuancée en luminosité */

        /* Murs : un quad vertical par côté de l'emprise, texturé en façade (UV réels,
           voir pushWallQuad et FACADE_TILE_*). */
        const float vTop = height / FACADE_TILE_H_M;
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j = (i + 1) % n;
            const float ex = px[j] - px[i];
            const float ez = pz[j] - pz[i];
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
            const float sideLen = std::sqrt(ex * ex + ez * ez);
            pushWallQuad(tb.v, tb.i, bi, bj, tj, ti, normal, wall, sideLen / FACADE_TILE_W_M, vTop);
        }

        /* Toit plat : éventail de triangles depuis le premier sommet (correct pour
           une emprise convexe, acceptable de loin pour les rares formes concaves). */
        const vec3 up{0.0f, 1.0f, 0.0f};
        for (std::size_t i = 1; i + 1 < n; ++i) {
            const vec3 a{px[0], top, pz[0]};
            const vec3 c{px[i], top, pz[i]};
            const vec3 d{px[i + 1], top, pz[i + 1]};
            pushTriangle(tb.v, tb.i, a, c, d, up, roof);
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
    if (skippedMonument > 0) {
        std::printf("[Buildings] %zu emprise(s) écartée(s) sous un monument 3D.\n",
                    skippedMonument);
    }
    if (m_count == 0) {
        return;
    }

    /* Concaténation des tuiles en un maillage unique : on garde, pour chacune, sa
       plage d'indices [firstIndex, +indexCount) et sa boîte englobante, pour ne
       dessiner au rendu que les tuiles visibles (voir draw). L'ordre des tuiles n'a
       pas d'importance ; seules les plages comptent. */
    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    verts.reserve(static_cast<std::size_t>(count) * 25);
    idx.reserve(static_cast<std::size_t>(count) * 45);
    for (const TileBuild& t : tiles) {
        if (t.i.empty()) {
            continue;
        }
        const auto vertexBase = static_cast<unsigned int>(verts.size());
        const int firstIndex = static_cast<int>(idx.size());
        verts.insert(verts.end(), t.v.begin(), t.v.end());
        for (const unsigned int ix : t.i) {
            idx.push_back(ix + vertexBase);
        }
        m_tiles.push_back(Tile{firstIndex, static_cast<int>(t.i.size()), t.mn, t.mx});
    }

    m_mesh = Mesh(verts, idx);
    std::printf("[Buildings] %zu bâtiments extrudés (%zu sommets, %zu tuiles).\n",
                m_count,
                verts.size(),
                m_tiles.size());
}

} /* namespace artouste::render */
