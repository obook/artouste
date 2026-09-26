/*
 * BuildingsMesh.cpp
 * Lit buildings.bin (emprises BD TOPO + hauteur) et extrude chaque bâtiment,
 * puis bridges.bin (axes d'ouvrages d'art + altitude de chaussée) et extrude
 * chaque tablier. Tout est fusionné en un maillage unique, calé sur le relief,
 * découpé en tuiles spatiales pour le culling.
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

/* Ce qu'il faut pour ranger un objet dans la bonne tuile : la grille et le
   calage du terrain qui la borne. Partagé par les deux lectures. */
struct Grille {
    std::vector<TileBuild>* tuiles;
    int                     cols;
    int                     rows;
    float                   halfW;
    float                   halfH;
    float                   origX;
    float                   origZ;

    /* Tuile où ranger un objet centré en (cx, cz), ou nullptr s'il tombe hors
       de l'emprise courante : une carte recadrée garde le fichier d'origine, et
       ce qui déborde flotterait au-delà du bord du relief. */
    [[nodiscard]] TileBuild* pour(float cx, float cz) const {
        if (std::fabs(cx - origX) > halfW || std::fabs(cz - origZ) > halfH) {
            return nullptr;
        }
        const int col = std::clamp(static_cast<int>((cx - origX + halfW) / TILE_M), 0, cols - 1);
        const int row = std::clamp(static_cast<int>((cz - origZ + halfH) / TILE_M), 0, rows - 1);
        return &(*tuiles)[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
                          static_cast<std::size_t>(col)];
    }
};

/* En-tête d'un fichier d'emprises : magie sur 4 octets, version, effectif. */
[[nodiscard]] bool lireEntete(std::ifstream& in, const char* attendue, std::uint32_t versionAttendue,
                              std::uint32_t& count) {
    char magic[4] = {0, 0, 0, 0};
    in.read(magic, 4);
    std::uint32_t version = 0;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    return in && magic[0] == attendue[0] && magic[1] == attendue[1] && magic[2] == attendue[2] &&
           magic[3] == attendue[3] && version == versionAttendue;
}

/* Bâtiments de la carte (buildings.bin, voir tools/fetch_buildings.py).
   Renvoie le nombre de volumes extrudés. */
[[nodiscard]] std::size_t chargerBatiments(const std::filesystem::path& dir,
                                           const Terrain&               terrain,
                                           const Grille&                grille) {
    const std::filesystem::path path = dir / "buildings.bin";
    std::ifstream               in(path, std::ios::binary);
    if (!in) {
        return 0; /* pas de bâtiments pour ce terrain */
    }
    std::uint32_t count = 0;
    if (!lireEntete(in, "ABLD", 1u, count)) {
        std::fprintf(stderr, "[Buildings] %s : en-tête invalide, bâtiments ignorés.\n",
                     path.string().c_str());
        return 0;
    }

    const Degagements degagements(terrain);
    const Ortho       ortho(dir / "ortho.jpg", terrain);

    /* Le filtre "bâtiment sur l'eau" ne vaut que pour les terrains de bord de
       mer. En montagne, le niveau 0 est juste le point le plus bas du relief :
       on y garde les cabanes et refuges, même au bord d'un lac d'altitude. */
    const bool filtrerEau = terrain.drawsSea();

    std::vector<float> px, pz; /* emprise en monde, réutilisée par bâtiment */
    std::size_t        faits           = 0;
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

        TileBuild* tb = grille.pour(cx, cz);
        if (tb == nullptr) {
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

        float minX = px[0], maxX = px[0], minZ = pz[0], maxZ = pz[0];
        for (std::size_t k = 0; k < px.size(); ++k) {
            minX = std::min(minX, px[k]);
            maxX = std::max(maxX, px[k]);
            minZ = std::min(minZ, pz[k]);
            maxZ = std::max(maxZ, pz[k]);
        }
        tb->mn.x = std::min(tb->mn.x, minX);
        tb->mx.x = std::max(tb->mx.x, maxX);
        tb->mn.z = std::min(tb->mn.z, minZ);
        tb->mx.z = std::max(tb->mx.z, maxZ);
        tb->mn.y = std::min(tb->mn.y, base);
        tb->mx.y = std::max(tb->mx.y, base + hauteur);

        /* Seed du mur décorrélée de celle du toit : les deux ne doivent pas
           varier de concert. */
        const vec3 mur  = WALL_COLOR * jitter(b * 2654435761u + 1u, 0.08f);
        const vec3 toit = ortho.couleurToit(cx, cz, maxX - minX, maxZ - minZ, b);

        extruderBatiment(tb->v, tb->i, px, pz, cx, cz, base, hauteur, mur, toit);
        ++faits;
    }

    if (ecartesEau > 0) {
        std::printf("[Buildings] %zu bâtiment(s) sur l'eau écarté(s) (cabanes au ras du bassin).\n",
                    ecartesEau);
    }
    if (ecartesMonument > 0) {
        std::printf("[Buildings] %zu emprise(s) écartée(s) sous un monument 3D.\n",
                    ecartesMonument);
    }
    return faits;
}

/* Béton d'ouvrage d'art. Un tablier ne se lit pas dans l'orthophoto comme un
   toit : au nadir on n'y voit que la chaussée et les voitures. */
const vec3 COULEUR_TABLIER{0.50f, 0.49f, 0.47f};

/* Photo effacée au-delà du bord du tablier, de chaque côté : le pont photographié
   déborde de la largeur estimée (parapets, ombre). À 0,5 m puis 1 m, le bord du
   pont d'Hossegor restait visible sur l'eau. */
constexpr float MARGE_EFFACEMENT_M = 2.5f;

/* Drapeau de bridges.bin : pont porté par un monument 3D, photo à effacer
   seulement. */
constexpr std::uint8_t PHOTO_SEULE = 1;

/* Ouvrages d'art de la carte (bridges.bin, voir tools/fetch_bridges.py). Le
   relief vient d'un modèle de TERRAIN, qui ignore les ponts : la photo plaquée
   dessus suit le creux du lit du fleuve et le tablier plonge dans l'eau. Les
   altitudes lues ici sont celles de la chaussée, relevées par la BD TOPO et
   recalées sur le sol du moteur. Les axes vont aussi dans segments, pour
   effacer la photo du pont peinte au sol. Renvoie le nombre de tabliers
   extrudés. */
[[nodiscard]] std::size_t chargerPonts(const std::filesystem::path& dir,
                                       const Terrain&               terrain,
                                       const Grille&                grille,
                                       std::vector<Vertex>&         vertsChaussee,
                                       std::vector<unsigned int>&   idxChaussee,
                                       std::vector<SegmentTablier>& segments) {
    const std::filesystem::path path = dir / "bridges.bin";
    std::ifstream               in(path, std::ios::binary);
    if (!in) {
        return 0; /* pas d'ouvrages relevés pour ce terrain */
    }
    std::uint32_t count = 0;
    if (!lireEntete(in, "ABRG", 2u, count)) {
        std::fprintf(stderr,
                     "[Buildings] %s : en-tête invalide ou ancienne version, ouvrages "
                     "ignorés (relancer tools/fetch_bridges.py).\n",
                     path.string().c_str());
        return 0;
    }

    /* Même drapage que le maillage du terrain (TerrainMaillage.cpp) : la
       chaussée montrera la photo du pont, alignée au sol qui l'entoure. */
    const CalageOrtho ortho{terrain.originX() - terrain.halfWidth(),
                            terrain.originZ() - terrain.halfHeight(),
                            2.0f * terrain.halfWidth(), 2.0f * terrain.halfHeight()};

    std::vector<float> px, py, pz; /* axe en monde, réutilisé par tablier */
    std::size_t        faits        = 0;
    std::size_t        sousMonument = 0;

    for (std::uint32_t b = 0; b < count; ++b) {
        /* Demi-largeurs de l'OUVRAGE, déjà mesurées par l'outil (emprise du
           pont, ou chaussée plus débord). */
        float         gauche   = 0.0f;
        float         droite   = 0.0f;
        std::uint8_t  drapeaux = 0;
        std::uint16_t npts     = 0;
        in.read(reinterpret_cast<char*>(&gauche), sizeof(gauche));
        in.read(reinterpret_cast<char*>(&droite), sizeof(droite));
        in.read(reinterpret_cast<char*>(&drapeaux), sizeof(drapeaux));
        in.read(reinterpret_cast<char*>(&npts), sizeof(npts));
        if (!in || npts < 2) {
            break; /* fichier tronqué ou axe dégénéré : on s'arrête */
        }

        px.clear();
        py.clear();
        pz.clear();
        for (std::uint16_t k = 0; k < npts; ++k) {
            float lon = 0.0f, lat = 0.0f, alt = 0.0f;
            in.read(reinterpret_cast<char*>(&lon), sizeof(lon));
            in.read(reinterpret_cast<char*>(&lat), sizeof(lat));
            in.read(reinterpret_cast<char*>(&alt), sizeof(alt));
            float x = 0.0f, z = 0.0f;
            terrain.worldAt(lon, lat, x, z);
            px.push_back(x);
            py.push_back(alt);
            pz.push_back(z);
        }
        if (!in) {
            break;
        }

        float minX = px[0], maxX = px[0], minZ = pz[0], maxZ = pz[0];
        float minY = py[0], maxY = py[0];
        for (std::size_t k = 1; k < px.size(); ++k) {
            minX = std::min(minX, px[k]);
            maxX = std::max(maxX, px[k]);
            minZ = std::min(minZ, pz[k]);
            maxZ = std::max(maxZ, pz[k]);
            minY = std::min(minY, py[k]);
            maxY = std::max(maxY, py[k]);
        }

        TileBuild* tb = grille.pour(0.5f * (minX + maxX), 0.5f * (minZ + maxZ));
        if (tb == nullptr) {
            continue;
        }

        /* La photo du pont s'efface dans tous les cas. */
        for (std::size_t k = 0; k + 1 < px.size(); ++k) {
            segments.push_back(SegmentTablier{vec2{px[k], pz[k]}, vec2{px[k + 1], pz[k + 1]},
                                              gauche + MARGE_EFFACEMENT_M,
                                              droite + MARGE_EFFACEMENT_M});
        }
        /* Mais pas de tablier là où un monument porte déjà le sien : le Grand
           Palais de Paris comprend le pont Alexandre-III, et les deux tabliers
           se superposaient à deux mètres d'écart. L'outil le déclare, le rayon
           du monument ne le dit pas (voir tools/fetch_bridges.py). */
        if ((drapeaux & PHOTO_SEULE) != 0) {
            ++sousMonument;
            continue;
        }

        const float demi = std::max(gauche, droite);
        tb->mn.x = std::min(tb->mn.x, minX - demi);
        tb->mx.x = std::max(tb->mx.x, maxX + demi);
        tb->mn.z = std::min(tb->mn.z, minZ - demi);
        tb->mx.z = std::max(tb->mx.z, maxZ + demi);
        tb->mn.y = std::min(tb->mn.y, minY - EPAISSEUR_TABLIER_M);
        tb->mx.y = std::max(tb->mx.y, maxY);

        extruderTablier(tb->v, tb->i, vertsChaussee, idxChaussee, px, py, pz, gauche, droite,
                        COULEUR_TABLIER, ortho);
        ++faits;
    }
    if (sousMonument > 0) {
        std::printf("[Buildings] %zu tablier(s) laissé(s) au monument 3D qui les couvre.\n",
                    sousMonument);
    }
    return faits;
}

} /* namespace */

Buildings::Buildings(const std::filesystem::path& dir, const Terrain& terrain) {
    const float halfW = terrain.halfWidth();
    const float halfH = terrain.halfHeight();

    const int cols = std::max(1, static_cast<int>((2.0f * halfW) / TILE_M) + 1);
    const int rows = std::max(1, static_cast<int>((2.0f * halfH) / TILE_M) + 1);
    std::vector<TileBuild> tiles(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));

    const Grille grille{&tiles, cols, rows, halfW, halfH, terrain.originX(), terrain.originZ()};

    /* Bâtiments et ouvrages d'art vont dans les mêmes tuiles : même matériau,
       même shader, un seul appel de dessin. Une carte peut n'avoir que les uns
       ou que les autres, d'où deux lectures indépendantes. */
    std::vector<Vertex>       chaussees;    /* faces du dessus, passe du terrain */
    std::vector<unsigned int> chausseesIdx;
    const std::size_t batiments = chargerBatiments(dir, terrain, grille);
    const std::size_t tabliers  = chargerPonts(dir, terrain, grille, chaussees, chausseesIdx,
                                               m_segments);
    if (!chausseesIdx.empty()) {
        m_tabliers = Mesh(chaussees, chausseesIdx);
    }

    m_count = batiments + tabliers;
    if (m_count == 0) {
        return;
    }

    /* Concaténation des tuiles en un maillage unique. On garde pour chacune sa
       plage d'indices et sa boîte englobante, pour ne dessiner au rendu que les
       tuiles visibles (voir draw). L'ordre des tuiles n'a pas d'importance. */
    std::vector<Vertex>       verts;
    std::vector<unsigned int> idx;
    verts.reserve(m_count * 25);
    idx.reserve(m_count * 45);
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
    std::printf("[Buildings] %zu bâtiments et %zu tabliers extrudés (%zu sommets, %zu tuiles).\n",
                batiments,
                tabliers,
                verts.size(),
                m_tiles.size());
}

} /* namespace artouste::render */
