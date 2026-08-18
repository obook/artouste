/*
 * BuildingsMesh.cpp
 * Lit buildings.bin (emprises BD TOPO + hauteur) et extrude chaque bâtiment.
 * Tout est fusionné en un maillage unique, calé sur le relief, découpé en
 * tuiles spatiales pour le culling.
 *
 * Couleurs dans BuildingsCouleurs, extrusion dans BuildingsGeometrie, zones
 * dégagées dans BuildingsEmprise. Le culling et le dessin sont dans
 * BuildingsDraw.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Buildings.hpp"

#include "render/Terrain.hpp"
#include "render/buildings/BuildingsCouleurs.hpp"
#include "render/buildings/BuildingsEmprise.hpp"
#include "render/buildings/BuildingsGeometrie.hpp"
#include "util/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

namespace artouste::render {

namespace {

/* Finesse de la grille de culling. Trop petit multiplie les tuiles (surcoût
   CPU par image), trop grand rend le culling grossier. */
constexpr float TILE_M = 2500.0f;

/* Géométrie accumulée d'une tuile, avec sa boîte englobante monde. */
struct TileBuild {
    std::vector<Vertex>       v;
    std::vector<unsigned int> i;
    vec3                      mn{1e30f, 1e30f, 1e30f};
    vec3                      mx{-1e30f, -1e30f, -1e30f};
};

/* En-tête de buildings.bin : "ABLD", version 1, nombre de bâtiments. */
[[nodiscard]] bool lireEntete(std::ifstream& in, std::uint32_t& count) {
    char magic[4] = {0, 0, 0, 0};
    in.read(magic, 4);
    std::uint32_t version = 0;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    return in && magic[0] == 'A' && magic[1] == 'B' && magic[2] == 'L' && magic[3] == 'D' &&
           version == 1u;
}

} /* namespace */

Buildings::Buildings(const std::filesystem::path& dir, const Terrain& terrain) {
    const std::filesystem::path path = dir / "buildings.bin";
    std::ifstream               in(path, std::ios::binary);
    if (!in) {
        return; /* pas de bâtiments pour ce terrain : maillage vide */
    }
    std::uint32_t count = 0;
    if (!lireEntete(in, count)) {
        std::fprintf(stderr, "[Buildings] %s : en-tête invalide, bâtiments ignorés.\n",
                     path.string().c_str());
        return;
    }

    const Degagements degagements(terrain);
    const Ortho       ortho(dir / "ortho.jpg", terrain);

    /* Le filtre "bâtiment sur l'eau" ne vaut que pour les terrains de bord de
       mer. En montagne, le niveau 0 est juste le point le plus bas du relief :
       on y garde les cabanes et refuges, même au bord d'un lac d'altitude. */
    const bool filtrerEau = terrain.drawsSea();

    const float halfW = terrain.halfWidth();
    const float halfH = terrain.halfHeight();
    const float origX = terrain.originX();
    const float origZ = terrain.originZ();

    const int cols = std::max(1, static_cast<int>((2.0f * halfW) / TILE_M) + 1);
    const int rows = std::max(1, static_cast<int>((2.0f * halfH) / TILE_M) + 1);
    std::vector<TileBuild> tiles(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));

    std::vector<float> px, pz; /* emprise en monde, réutilisée par bâtiment */
    std::size_t        ecartesEau      = 0;
    std::size_t        ecartesMonument = 0;

    for (std::uint32_t b = 0; b < count; ++b) {
        float         hauteur = 0.0f;
        std::uint16_t npts    = 0;
        in.read(reinterpret_cast<char*>(&hauteur), sizeof(hauteur));
        in.read(reinterpret_cast<char*>(&npts), sizeof(npts));
        if (!in || npts < 3) {
            break; /* fichier tronqué ou bâtiment dégénéré : on s'arrête */
        }

        px.clear();
        pz.clear();
        float base = 1e9f; /* altitude du sol la plus basse sous l'emprise */
        float cx   = 0.0f;
        float cz   = 0.0f;
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

        /* Hors de l'emprise courante (carte recadrée) : il flotterait au-delà
           du bord du relief. */
        if (std::fabs(cx - origX) > halfW || std::fabs(cz - origZ) > halfH) {
            continue;
        }
        if (filtrerEau && ortho.surLeau(cx, cz, base)) {
            ++ecartesEau;
            continue;
        }
        if (degagements.surUnPad(px, pz, cx, cz)) {
            continue;
        }
        if (degagements.sousUnMonument(px, pz, cx, cz)) {
            ++ecartesMonument;
            continue;
        }

        /* Tuile de ce bâtiment, d'après son centre, et sa boîte englobante. */
        const int  col = std::clamp(static_cast<int>((cx - origX + halfW) / TILE_M), 0, cols - 1);
        const int  row = std::clamp(static_cast<int>((cz - origZ + halfH) / TILE_M), 0, rows - 1);
        TileBuild& tb  = tiles[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
                              static_cast<std::size_t>(col)];

        float minX = px[0], maxX = px[0], minZ = pz[0], maxZ = pz[0];
        for (std::size_t k = 0; k < px.size(); ++k) {
            minX = std::min(minX, px[k]);
            maxX = std::max(maxX, px[k]);
            minZ = std::min(minZ, pz[k]);
            maxZ = std::max(maxZ, pz[k]);
        }
        tb.mn.x = std::min(tb.mn.x, minX);
        tb.mx.x = std::max(tb.mx.x, maxX);
        tb.mn.z = std::min(tb.mn.z, minZ);
        tb.mx.z = std::max(tb.mx.z, maxZ);
        tb.mn.y = std::min(tb.mn.y, base);
        tb.mx.y = std::max(tb.mx.y, base + hauteur);

        /* Seed du mur décorrélée de celle du toit : les deux ne doivent pas
           varier de concert. */
        const vec3 mur  = WALL_COLOR * jitter(b * 2654435761u + 1u, 0.08f);
        const vec3 toit = ortho.couleurToit(cx, cz, maxX - minX, maxZ - minZ, b);

        extruderBatiment(tb.v, tb.i, px, pz, cx, cz, base, hauteur, mur, toit);
        ++m_count;
    }

    if (ecartesEau > 0) {
        std::printf("[Buildings] %zu bâtiment(s) sur l'eau écarté(s) (cabanes au ras du bassin).\n",
                    ecartesEau);
    }
    if (ecartesMonument > 0) {
        std::printf("[Buildings] %zu emprise(s) écartée(s) sous un monument 3D.\n",
                    ecartesMonument);
    }
    if (m_count == 0) {
        return;
    }

    /* Concaténation des tuiles en un maillage unique. On garde pour chacune sa
       plage d'indices et sa boîte englobante, pour ne dessiner au rendu que les
       tuiles visibles (voir draw). L'ordre des tuiles n'a pas d'importance. */
    std::vector<Vertex>       verts;
    std::vector<unsigned int> idx;
    verts.reserve(static_cast<std::size_t>(count) * 25);
    idx.reserve(static_cast<std::size_t>(count) * 45);
    for (const TileBuild& t : tiles) {
        if (t.i.empty()) {
            continue;
        }
        const auto vertexBase = static_cast<unsigned int>(verts.size());
        const int  firstIndex = static_cast<int>(idx.size());
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
