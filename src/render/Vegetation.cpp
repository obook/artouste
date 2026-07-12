/*
 * Vegetation.cpp
 * Sème des arbres en billboards sur le terrain à partir de son orthophoto, puis
 * les dessine par instanciation GPU. Un arbre est retenu là où le sol est vert
 * (signature de couleur de la forêt dans l'ortho) et sous la limite forestière ;
 * il est posé sur le relief (heightAt). Toutes les positions sont téléversées une
 * fois dans un tampon d'instances ; le rendu dessine un unique quad autant de
 * fois qu'il y a d'arbres.
 *
 * Prototype : semis simple et brume identique aux bâtiments, pour juger le rendu
 * et les performances avant un pipeline hors-ligne et des niveaux de détail.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/Vegetation.hpp"

#include "render/Terrain.hpp"

#include <glad/glad.h>

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace artouste::render {

namespace {

/* Espacement de la grille de semis (m) : un arbre candidat tous les SPACING_M,
   avec une position perturbée dans la maille. Comme chaque maille ne donne au plus
   qu'un arbre, l'espacement fixe la densité maximale (1 arbre par SPACING_M^2) :
   à 8 m -> ~150 arbres/ha, un couvert crédible sans exploser le nombre. Réglable
   via ARTOUSTE_TREE_SPACING pour tâtonner sans recompiler (plus petit = plus dense
   = plus lourd ; une vraie forêt tourne autour de 4-5 m). */
constexpr float SPACING_M_DEFAULT = 8.0f;

/* Limite forestière progressive (m). Une forêt de montagne ne s'arrête pas net :
   elle se raréfie (pins isolés) avant de céder la place à la pelouse d'altitude. On
   garde donc le couvert plein sous TREELINE_FULL, on raréfie linéairement jusqu'à
   TREELINE_MAX, et plus rien au-dessus. Évite la ligne de coupure brutale que
   donnait une limite unique, et pose des arbres épars sur les hautes pentes vertes.
   Pyrénées : couvert dense jusque ~1900 m, derniers pins à crochets vers ~2200 m. */
constexpr float TREELINE_FULL = 1900.0f;
constexpr float TREELINE_MAX  = 2200.0f;

/* Taille des arbres : largeur du billboard (m) ; la hauteur en découle dans le
   shader (facteur ASPECT). Panachée par arbre pour éviter l'uniformité. */
constexpr float TREE_WIDTH_MIN = 4.0f;
constexpr float TREE_WIDTH_MAX = 7.0f;

/* On dégage un disque autour du point de départ pour ne pas noyer l'hélisurface
   d'arbres (rayon au carré, m^2). */
constexpr float CLEAR_R2 = 30.0f * 30.0f;

/* Petit dégagement de secours autour du repère d'un lac (rayon au carré, m^2),
   utilisé seulement si le remplissage de proche en proche (flood fill) ne trouve
   pas d'eau sous le repère (repère mal placé). Le masque d'eau, lui, épouse la vraie
   forme du plan d'eau, quelle que soit sa taille. */
constexpr float LAKE_FALLBACK_R2 = 120.0f * 120.0f;

/* Nombre d'espèces de l'atlas trees_atlas.png (sapin, feuillu, mélèze). Doit
   correspondre à ATLAS_COUNT dans vegetation.vert. */
constexpr int NUM_SPECIES = 3;
static_assert(NUM_SPECIES == 3, "trees_atlas.png et ATLAS_COUNT (vegetation.vert) : 3 espèces");

/* Garde-fou : plafond du nombre d'arbres, pour éviter une explosion mémoire si
   la grille est réglée très fine (6 M x 20 octets ~ 120 Mo de tampon d'instances).
   Le balayage étant nord -> sud, atteindre ce plafond tronquerait le semis au sud :
   on avertit alors et on invite à agrandir l'espacement. */
constexpr std::size_t MAX_TREES = 6'000'000;

/* Budget d'arbres : au-delà, on éclaircit uniformément le semis (chaque arbre gardé
   avec la même probabilité, de façon déterministe) pour tenir la charge sur les
   grandes cartes très boisées, où les billboards croisés génèrent beaucoup de
   surdessin (Bordeaux : ~5 M d'arbres). Ossau (~1,1 M) reste sous le budget, inchangé.
   Réglable par ARTOUSTE_TREE_MAX. */
constexpr std::size_t TARGET_TREES = 1'600'000;

/* Générateur pseudo-aléatoire déterministe (LCG) à partir d'une graine entière :
   même semis d'une exécution à l'autre (pas de scintillement, résultats stables). */
std::uint32_t hashU32(std::uint32_t x) {
    x ^= 0x9e3779b9u;
    x = x * 1664525u + 1013904223u;
    x ^= x >> 16;
    x = x * 2654435761u;
    return x;
}

/* Réel dans [0,1) tiré d'une graine. */
float unitOf(std::uint32_t seed) {
    return static_cast<float>(hashU32(seed) >> 8) / 16777216.0f;
}

/* Signature de couleur de la forêt dans l'orthophoto (canaux normalisés 0..1) :
   vert dominant, ni trop sombre (ombres), ni trop clair (prairies, champs), plus vert
   que bleu. Seuils élargis après diagnostic (carte de couverture sur Ossau) : le
   filtre d'origine (g >= r*1.02, g < 0.44) laissait des trous dans de vraies forêts,
   surtout sur les versants ensoleillés (verts jaunâtres) ; on inclut donc ces verts
   plus jaunes et un peu plus clairs, tout en laissant la roche, l'éboulis et l'eau
   non couverts. */
bool looksLikeForest(float r, float g, float b) {
    /* Zone claire ou blanche (grève, gravier, roche/neige au soleil, chemin) : pas
       d'arbre. On l'écarte sur la luminance, au-dessus du vert sombre de la forêt. */
    const float lum = 0.299f * r + 0.587f * g + 0.114f * b;
    if (lum > 0.46f) {
        return false;
    }
    return g > 0.12f && g < 0.48f && g >= r * 0.97f && g >= b * 1.08f && b < 0.48f;
}

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

Vegetation::Vegetation(const std::filesystem::path& terrainDir, const Terrain& terrain,
                       const std::filesystem::path& spritePath) {
    /* Sprite d'arbre : sans lui, rien à dessiner. */
    m_sprite = Texture(spritePath);
    if (!m_sprite.valid()) {
        std::fprintf(stderr, "[Vegetation] sprite absent (%s) : végétation ignorée.\n",
                     spritePath.string().c_str());
        return;
    }

    /* Orthophoto côté CPU (mêmes conventions que Buildings : rangée 0 = nord). */
    const std::filesystem::path orthoPath = terrainDir / "ortho.jpg";
    int            orthoW = 0, orthoH = 0, orthoCh = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* ortho = stbi_load(orthoPath.string().c_str(), &orthoW, &orthoH, &orthoCh, 3);
    if (ortho == nullptr) {
        std::fprintf(stderr, "[Vegetation] orthophoto absente (%s) : végétation ignorée.\n",
                     orthoPath.string().c_str());
        return;
    }

    /* Espacement de la grille, éventuellement forcé par l'environnement. */
    float spacing = SPACING_M_DEFAULT;
    if (const char* env = std::getenv("ARTOUSTE_TREE_SPACING"); env != nullptr && env[0] != '\0') {
        const float v = std::strtof(env, nullptr);
        if (v >= 4.0f) {
            spacing = v;
        }
    }

    const float halfW = terrain.halfWidth();
    const float halfH = terrain.halfHeight();
    const bool  clear = terrain.hasStart();
    const float sx    = clear ? terrain.startX() : 0.0f;
    const float sz    = clear ? terrain.startZ() : 0.0f;

    /* Conversion d'une position monde (x est, z sud) en pixel de l'orthophoto
       (colonne 0 = ouest, rangée 0 = nord), utilisée pour le masque d'eau et la
       lecture de couleur. */
    auto toPixel = [&](float x, float z, int& ox, int& oy) {
        const float u = (x + halfW) / (2.0f * halfW);
        const float v = (z + halfH) / (2.0f * halfH);
        ox = std::min(std::max(static_cast<int>(u * static_cast<float>(orthoW - 1)), 0), orthoW - 1);
        oy = std::min(std::max(static_cast<int>(v * static_cast<float>(orthoH - 1)), 0), orthoH - 1);
    };
    auto orthoRGB = [&](int ox, int oy, float& r, float& g, float& b) {
        const unsigned char* p = ortho
                               + (static_cast<std::size_t>(oy) * static_cast<std::size_t>(orthoW)
                                  + static_cast<std::size_t>(ox)) * 3;
        r = p[0] / 255.0f;
        g = p[1] / 255.0f;
        b = p[2] / 255.0f;
    };

    /* Masque d'eau : à partir de chaque repère "Lac" de landmarks.txt, on propage de
       proche en proche (flood fill) sur les pixels d'eau connectés, ce qui épouse la
       forme réelle du plan d'eau (un simple disque ratait les lacs allongés comme le
       réservoir de Fabrèges). Les lacs dont le repère n'est pas sur de l'eau (repère
       mal placé) sont rangés à part, dégagés ensuite par un petit disque de secours. */
    std::vector<unsigned char>           water(static_cast<std::size_t>(orthoW)
                                               * static_cast<std::size_t>(orthoH), 0);
    std::vector<std::pair<float, float>> fallbackLakes;
    constexpr std::size_t                FLOOD_CAP = 500'000;  /* borne par lac (anti-emballement) */
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
            float lx = 0.0f, lz = 0.0f;
            terrain.worldAt(lon, lat, lx, lz);
            int cx = 0, cy = 0;
            toPixel(lx, lz, cx, cy);

            /* Graine du remplissage : un pixel d'eau proche du repère (le repère
               peut être posé un peu à côté du plan d'eau). */
            int seedX = -1, seedY = -1;
            for (int dy = -12; dy <= 12 && seedX < 0; ++dy) {
                for (int dx = -12; dx <= 12; ++dx) {
                    const int px = std::min(std::max(cx + dx, 0), orthoW - 1);
                    const int py = std::min(std::max(cy + dy, 0), orthoH - 1);
                    float r = 0.0f, g = 0.0f, b = 0.0f;
                    orthoRGB(px, py, r, g, b);
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
                    orthoRGB(n[0], n[1], r, g, b);
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

    /* Masque des bâtiments : on ne plante pas d'arbre sur une emprise de bâtiment
       (sinon des arbres poussent sur les toits dans les villages, là où l'ortho est
       verte entre les maisons). On lit buildings.bin (mêmes emprises que render::
       Buildings), on rastérise chaque polygone dans un masque à la résolution de
       l'ortho, avec un pixel de marge pour que le billboard ne mange pas les murs.
       Format : magie "ABLD", version, nombre, puis par bâtiment hauteur(float),
       n(uint16), n x (lon,lat float). */
    std::vector<unsigned char> building(static_cast<std::size_t>(orthoW)
                                        * static_cast<std::size_t>(orthoH), 0);
    {
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
                float lx = 0.0f, lz = 0.0f;
                terrain.worldAt(lon, lat, lx, lz);
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
    }

    /* Un arbre = centre (x, y, z) + largeur + espèce + azimut, empaqueté en six
       flottants pour coller au format d'instance attendu par le shader. */
    std::vector<float> instances;
    const int cols = std::max(1, static_cast<int>((2.0f * halfW) / spacing));
    const int rows = std::max(1, static_cast<int>((2.0f * halfH) / spacing));
    instances.reserve(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));

    for (int r = 0; r < rows && m_count < MAX_TREES; ++r) {
        for (int c = 0; c < cols && m_count < MAX_TREES; ++c) {
            /* Graine stable propre à la maille : perturbation de position et
               largeur reproductibles. */
            const std::uint32_t seed = hashU32(static_cast<std::uint32_t>(r) * 73856093u
                                               ^ static_cast<std::uint32_t>(c) * 19349663u);
            const float jx = (unitOf(seed) - 0.5f) * spacing;
            const float jz = (unitOf(seed ^ 0x5bd1e995u) - 0.5f) * spacing;
            const float x  = -halfW + (static_cast<float>(c) + 0.5f) * spacing + jx;
            const float z  = -halfH + (static_cast<float>(r) + 0.5f) * spacing + jz;
            if (x < -halfW || x > halfW || z < -halfH || z > halfH) {
                continue;
            }

            /* Dégagement autour de l'hélisurface de départ. */
            if (clear) {
                const float dx = x - sx, dz = z - sz;
                if (dx * dx + dz * dz < CLEAR_R2) {
                    continue;
                }
            }

            /* Pixel de l'ortho sous le point perturbé (eau et couleur du sol). */
            int ox = 0, oy = 0;
            toPixel(x, z, ox, oy);

            /* Eau (masque du plan d'eau) ou bâtiment (emprise) : aucun arbre dessus. */
            const std::size_t maskIdx = static_cast<std::size_t>(oy) * static_cast<std::size_t>(orthoW)
                                        + static_cast<std::size_t>(ox);
            if (water[maskIdx] != 0 || building[maskIdx] != 0) {
                continue;
            }

            /* Lacs sans graine d'eau (repère mal placé) : petit disque de secours. */
            if (!fallbackLakes.empty()) {
                bool onLake = false;
                for (const auto& lake : fallbackLakes) {
                    const float dx = x - lake.first, dz = z - lake.second;
                    if (dx * dx + dz * dz < LAKE_FALLBACK_R2) {
                        onLake = true;
                        break;
                    }
                }
                if (onLake) {
                    continue;
                }
            }

            /* Couleur du sol : on ne plante que sur du vert de forêt. */
            float cr = 0.0f, cg = 0.0f, cb = 0.0f;
            orthoRGB(ox, oy, cr, cg, cb);
            if (!looksLikeForest(cr, cg, cb)) {
                continue;
            }

            /* Altitude : posé sur le relief. Au-dessus de la limite forestière, le
               couvert se raréfie progressivement (transition forêt -> pelouse) plutôt
               que de s'arrêter net ; sous le niveau de la mer, rien (terrains côtiers). */
            const float y = terrain.heightAt(x, z);
            if (y > TREELINE_MAX) {
                continue;
            }
            if (y > TREELINE_FULL) {
                const float keep = (TREELINE_MAX - y) / (TREELINE_MAX - TREELINE_FULL);
                if (unitOf(seed ^ 0x94d049bbu) > keep) {
                    continue;  /* raréfaction croissante vers le haut */
                }
            }
            if (terrain.drawsSea() && y < 0.5f) {
                continue;
            }

            const float width = TREE_WIDTH_MIN
                              + unitOf(seed ^ 0x2545f491u) * (TREE_WIDTH_MAX - TREE_WIDTH_MIN);

            /* Espèce selon l'altitude et le hasard : sapin (0) de plus en plus
               fréquent en montant, mélèze (2) à l'étage supérieur, feuillu (1)
               dominant plus bas. */
            const float t  = std::clamp((y - 1000.0f) / 700.0f, 0.0f, 1.0f);
            const float r1 = unitOf(seed ^ 0x27d4eb2fu);
            const float r2 = unitOf(seed ^ 0x165667b1u);
            float       species = 1.0f;  /* feuillu par défaut */
            if (r1 < 0.30f + 0.45f * t) {
                species = 0.0f;          /* sapin */
            } else if (t > 0.5f && r2 < 0.6f) {
                species = 2.0f;          /* mélèze */
            }

            /* Azimut de la croix : oriente les deux quads perpendiculaires, décorrélé
               d'un arbre à l'autre pour éviter des rangées alignées. */
            const float azimuth = unitOf(seed ^ 0x68bc21ebu) * 6.2831853f;

            instances.push_back(x);
            instances.push_back(y);
            instances.push_back(z);
            instances.push_back(width);
            instances.push_back(species);
            instances.push_back(azimuth);
            ++m_count;
        }
    }

    stbi_image_free(ortho);

    if (m_count >= MAX_TREES) {
        std::fprintf(stderr,
                     "[Vegetation] plafond de %zu arbres atteint : semis tronqué au sud. "
                     "Agrandissez ARTOUSTE_TREE_SPACING.\n",
                     MAX_TREES);
    }

    if (m_count == 0) {
        std::printf("[Vegetation] aucun arbre semé (pas de forêt détectée).\n");
        return;
    }

    /* Budget : au-delà, éclaircissement uniforme (échantillonnage déterministe par
       indice), sans troncature spatiale, pour limiter le surdessin des grandes cartes. */
    std::size_t budget = TARGET_TREES;
    if (const char* env = std::getenv("ARTOUSTE_TREE_MAX"); env != nullptr && env[0] != '\0') {
        const long v = std::strtol(env, nullptr, 10);
        if (v > 0) {
            budget = static_cast<std::size_t>(v);
        }
    }
    if (m_count > budget) {
        const float        keep = static_cast<float>(budget) / static_cast<float>(m_count);
        std::vector<float> thinned;
        thinned.reserve(budget * 6 + 6);
        for (std::size_t i = 0; i < m_count; ++i) {
            if (unitOf(static_cast<std::uint32_t>(i) ^ 0x2b1f5c3du) < keep) {
                for (int k = 0; k < 6; ++k) {
                    thinned.push_back(instances[i * 6 + static_cast<std::size_t>(k)]);
                }
            }
        }
        std::printf("[Vegetation] semis éclairci : %zu -> %zu arbres (budget).\n", m_count,
                    thinned.size() / 6);
        instances.swap(thinned);
        m_count = instances.size() / 6;
    }

    /* Géométrie de base : deux quads verticaux (billboard en croix). Par sommet :
       coin (x dans [-0.5,0.5], y dans [0,1]), UV, et plan (0 ou 1). Le shader
       oriente chaque plan dans le monde selon l'azimut de l'instance. */
    const float quads[] = {
        /* corner.x corner.y   u    v   plane */
        /* plan 0 */
        -0.5f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.5f, 1.0f, 1.0f, 1.0f, 0.0f,
        -0.5f, 1.0f, 0.0f, 1.0f, 0.0f,
        /* plan 1 (perpendiculaire, posé par le shader) */
        -0.5f, 0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, 1.0f, 1.0f, 1.0f, 1.0f,
        -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,
    };
    const unsigned int indices[] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    /* Sommets : attributs 0 (coin), 1 (UV), 2 (plan). Cinq flottants par sommet. */
    constexpr GLsizei vstride = 5 * sizeof(float);
    glGenBuffers(1, &m_quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quads), quads, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vstride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vstride,
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, vstride,
                          reinterpret_cast<void*>(4 * sizeof(float)));

    /* Indices (stockés dans le VAO). */
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    /* Tampon d'instances : attributs 3 (centre), 4 (largeur), 5 (espèce), 6 (azimut),
       un par arbre. Six flottants par instance (stride 24 octets). */
    constexpr GLsizei istride = 6 * sizeof(float);
    glGenBuffers(1, &m_instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(instances.size() * sizeof(float)), instances.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, istride, reinterpret_cast<void*>(0));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, istride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glVertexAttribDivisor(4, 1);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, istride,
                          reinterpret_cast<void*>(4 * sizeof(float)));
    glVertexAttribDivisor(5, 1);
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, istride,
                          reinterpret_cast<void*>(5 * sizeof(float)));
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    std::printf("[Vegetation] %zu arbres semés (espacement %.0f m).\n", m_count,
                static_cast<double>(spacing));
}

Vegetation::~Vegetation() {
    release();
}

void Vegetation::release() noexcept {
    if (m_instanceVbo != 0) {
        glDeleteBuffers(1, &m_instanceVbo);
    }
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
    }
    if (m_quadVbo != 0) {
        glDeleteBuffers(1, &m_quadVbo);
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    m_vao = m_quadVbo = m_instanceVbo = m_ebo = 0;
    m_count = 0;
}

void Vegetation::draw() const {
    if (m_count == 0) {
        return;
    }
    m_sprite.bind(0);
    glBindVertexArray(m_vao);
    /* 12 indices = les deux quads de la croix ; une instance par arbre. */
    glDrawElementsInstanced(GL_TRIANGLES, 12, GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(m_count));
    glBindVertexArray(0);
}

}  /* namespace artouste::render */
