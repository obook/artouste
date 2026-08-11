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

/* Toitures : la couleur est lue dans l'orthophoto de la carte, au centre de
   l'emprise (voir roofFromOrtho). Une prise de vue au nadir montre le toit lui-même
   à cet endroit : chaque bâtiment reçoit donc sa vraie teinte, le zinc parisien
   comme la tuile landaise, sans rien déclarer par carte.

   La palette ci-dessous ne sert plus que de repli, quand l'orthophoto est absente
   ou illisible : quelques teintes panachées, la tuile terre cuite dominante (3
   entrées sur 6), choisies par bâtiment de façon stable (voir pickRoof). */
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

/* Teinte de toiture depuis un pixel de l'orthophoto (canaux 0..255, RGB).
   ROOF_GAIN : le toit reçoit un éclairage plus sombre que le sol (ambiant 0.4 et
   normale verticale dans building.frag, contre 0.55 et une pente moyenne dans
   terrain.frag). Sans compensation, un bâtiment se lirait comme une tache sombre
   sur sa propre empreinte dans l'ortho.
   ROOF_MIN / ROOF_MAX : bornes de luminosité. Une cour à l'ombre ou un pixel pris
   dans l'ombre portée d'une tour ne doit pas donner un toit noir, et une verrière
   surexposée pas un toit blanc : ni l'un ni l'autre ne se lit comme une toiture.
   ROOF_TARGET_SAT : les orthophotos n'ont pas toutes le même mordant. Celle de
   Paris est grise (saturation moyenne des pixels 0.024, soit six niveaux sur 255)
   et, reportée telle quelle, elle donne une nappe de toits uniformes ; celle de
   Dax est franche (0.052) et n'a besoin de rien. On étale donc les valeurs autour
   de la luminosité moyenne de CETTE ortho, d'autant plus que sa saturation est
   faible, jamais en deçà de 1 (on ne comprime pas une image déjà franche) ni
   au-delà de ROOF_CONTRAST_MAX (au delà, les toits clairs virent au blanc et les
   sombres au noir : on fabrique du contraste au lieu d'en révéler).
   ROOF_TINT : nuance chaude ou froide tirée par bâtiment. L'ortho ne peut pas la
   donner, elle est quasi achromatique ; sans elle une ville de zinc vire au gris
   d'atelier. Faible à dessein : on cherche le chatoiement des toitures, pas un
   panachage de couleurs qui n'existent pas sur place. */
constexpr float ROOF_GAIN = 1.10f;
constexpr float ROOF_MIN = 0.18f;
constexpr float ROOF_MAX = 0.85f;
constexpr float ROOF_TARGET_SAT = 0.05f;
constexpr float ROOF_CONTRAST_MAX = 1.35f;
constexpr float ROOF_TINT = 0.06f;

/* Luminosité et saturation moyennes d'une orthophoto, sur un échantillon (un pixel
   sur ORTHO_STEP dans chaque direction : quelques centaines de milliers de points
   suffisent à mesurer une teinte d'ensemble, inutile de lire les treize millions). */
struct OrthoStats {
    float luminosite = 0.45f;
    float saturation = ROOF_TARGET_SAT;
};

OrthoStats statsOrtho(const unsigned char* ortho, int w, int h) {
    constexpr int ORTHO_STEP = 8;
    if (ortho == nullptr || w <= 0 || h <= 0) {
        return {};
    }
    double sommeLum = 0.0, sommeSat = 0.0;
    std::size_t n = 0;
    for (int y = 0; y < h; y += ORTHO_STEP) {
        for (int x = 0; x < w; x += ORTHO_STEP) {
            const unsigned char* p =
                ortho + (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                         static_cast<std::size_t>(x)) *
                            3;
            sommeLum += (0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]) / 255.0;
            const int haut = std::max({p[0], p[1], p[2]});
            const int bas = std::min({p[0], p[1], p[2]});
            sommeSat += (haut - bas) / 255.0;
            ++n;
        }
    }
    if (n == 0) {
        return {};
    }
    return OrthoStats{static_cast<float>(sommeLum / static_cast<double>(n)),
                      static_cast<float>(sommeSat / static_cast<double>(n))};
}

/* Le pixel ne vaut comme couleur de toit que si le toit est plus grand que lui.
   Les orthophotos vont de 0.85 m/pixel (Dax) à 9.8 m/pixel (Arcachon) : au pied de
   cette échelle un pixel couvre la maison, son jardin et ses pins, et la teinte lue
   n'est plus une toiture mais une bouillie. On n'échantillonne donc qu'à partir
   d'une emprise de deux pixels de côté ; en dessous, la palette de repli reprend
   la main. */
constexpr float ROOF_ORTHO_MIN_PX = 2.0f;

vec3 roofFromOrtho(const unsigned char* px, float pivot, float contraste) {
    const auto canal = [pivot, contraste](unsigned char c) {
        const float v = static_cast<float>(c) / 255.0f * ROOF_GAIN;
        return std::clamp(pivot + (v - pivot) * contraste, ROOF_MIN, ROOF_MAX);
    };
    return vec3{canal(px[0]), canal(px[1]), canal(px[2])};
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
   réels : u parcourt la largeur du côté (0 à u1, soit sa longueur /
   FACADE_TILE_W_M), v la hauteur depuis le sol du bâtiment (0 à sa hauteur /
   FACADE_TILE_H_M). La texture de façade (voir building.frag) se répète ainsi à
   échelle constante quelle que soit la taille du bâtiment. Un u1 négatif
   demande la tuile aveugle plutôt que la tuile fenêtrée (voir l'appelant) ; la
   répétition, elle, est indifférente au signe. */
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

    /* Orthophoto chargée côté CPU : on y lit la couleur du sol sous chaque bâtiment,
       pour teinter son toit (roofFromOrtho) et pour écarter les cabanes bâties sur
       l'eau. Absente ou illisible : toitures de repli (ROOF_PALETTE) et filtre eau
       inopérant, comme sur une carte sans ortho. */
    int orthoW = 0, orthoH = 0, orthoCh = 0;
    stbi_set_flip_vertically_on_load(0); /* rangée 0 = nord, comme le relief */
    unsigned char* ortho =
        stbi_load((dir / "ortho.jpg").string().c_str(), &orthoW, &orthoH, &orthoCh, 3);
    const float halfW = terrain.halfWidth();
    const float halfH = terrain.halfHeight();
    const float origX = terrain.originX(); /* centre de l'emprise (0 sauf carte recadrée) */
    const float origZ = terrain.originZ();

    /* Pixel de l'orthophoto sous une position monde (nullptr si pas d'ortho) :
       demi-dimensions du terrain pour convertir, origine au centre du bloc. */
    const auto orthoPixel = [&](float x, float z) -> const unsigned char* {
        if (ortho == nullptr) {
            return nullptr;
        }
        const float u = (x - origX + halfW) / (2.0f * halfW); /* 0 = ouest, 1 = est */
        const float v = (z - origZ + halfH) / (2.0f * halfH); /* 0 = nord,  1 = sud */
        const int ox =
            std::clamp(static_cast<int>(u * static_cast<float>(orthoW - 1)), 0, orthoW - 1);
        const int oy =
            std::clamp(static_cast<int>(v * static_cast<float>(orthoH - 1)), 0, orthoH - 1);
        return ortho + (static_cast<std::size_t>(oy) * static_cast<std::size_t>(orthoW) +
                        static_cast<std::size_t>(ox)) *
                           3;
    };

    /* Taille au sol d'un pixel d'orthophoto (mètres), pour savoir si l'emprise d'un
       bâtiment est assez grande pour que sa couleur y soit lisible (ROOF_ORTHO_MIN_PX). */
    const float pixelX = ortho != nullptr ? 2.0f * halfW / static_cast<float>(orthoW) : 0.0f;
    const float pixelZ = ortho != nullptr ? 2.0f * halfH / static_cast<float>(orthoH) : 0.0f;

    /* Mordant de cette orthophoto : il règle l'étalement des couleurs de toiture
       (voir ROOF_TARGET_SAT), une image grise étant écartée plus fort qu'une image
       franche. Mesuré une fois par carte, au chargement. */
    const OrthoStats stats = statsOrtho(ortho, orthoW, orthoH);
    const float contrasteToit =
        std::clamp(ROOF_TARGET_SAT / std::max(stats.saturation, 0.005f), 1.0f, ROOF_CONTRAST_MAX);
    if (ortho != nullptr) {
        std::printf("[Buildings] ortho : luminosité moyenne %.3f, saturation moyenne %.3f "
                    "-> contraste des toitures x%.2f\n",
                    static_cast<double>(stats.luminosite),
                    static_cast<double>(stats.saturation),
                    static_cast<double>(contrasteToit));
    }

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
        if (filterWater && base <= WATER_ALT_M) {
            if (const unsigned char* p = orthoPixel(cx, cz); p != nullptr) {
                const float r = p[0] / 255.0f;
                const float bl = p[2] / 255.0f;
                if (r <= WATER_RED && (bl - r) >= WATER_BLUE_BIAS) {
                    ++skippedWater;
                    continue;
                }
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
        float minX = px[0], maxX = px[0], minZ = pz[0], maxZ = pz[0];
        for (std::size_t k = 0; k < n; ++k) {
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
        tb.mx.y = std::max(tb.mx.y, top);

        /* Seed décorrélée de celle du toit (voir pickRoof) : mur et toit ne doivent
           pas varier de concert. */
        const float wj = jitter(b * 2654435761u + 1u, 0.08f);
        const vec3 wall = WALL_COLOR * wj;
        /* Toit : sa teinte réelle, lue dans l'orthophoto au centre de l'emprise, dès
           que l'emprise couvre assez de pixels pour que ce soit bien le toit qu'on y
           lise (voir ROOF_ORTHO_MIN_PX). Emprise trop petite, ortho absente : palette
           de repli. Dans les deux cas la nuance de luminosité par bâtiment est
           conservée : un pâté d'immeubles voisins tombe souvent sur le même pixel
           d'ortho, et sans elle les toits s'aplatiraient en une nappe unie. */
        const bool toitLisible = maxX - minX >= ROOF_ORTHO_MIN_PX * pixelX &&
                                 maxZ - minZ >= ROOF_ORTHO_MIN_PX * pixelZ;
        const unsigned char* pixelToit = toitLisible ? orthoPixel(cx, cz) : nullptr;
        vec3 roof = (pixelToit != nullptr
                         ? roofFromOrtho(pixelToit, stats.luminosite, contrasteToit)
                         : pickRoof(b)) *
                    jitter(b, 0.12f);
        /* Nuance chaude ou froide (voir ROOF_TINT) : le rouge et le bleu tirés en sens
           inverse, ce qui fait basculer la teinte sans toucher à sa luminosité. */
        const float chaud = jitter(b * 40503u + 3u, ROOF_TINT);
        roof.x *= chaud;
        roof.z /= chaud;

        /* Murs : un quad vertical par côté de l'emprise, texturé en façade (UV réels,
           voir pushWallQuad et FACADE_TILE_*).

           Toutes les faces ne sont pas fenêtrées : un bâtiment réel montre ses
           fenêtres sur ses deux longues faces et garde ses pignons pleins. On
           prend pour axe de référence le côté le plus long de l'emprise ; une
           face dont la normale s'écarte de moins de 45 degrés de celle de cet
           axe est fenêtrée, les autres sont pleines. Deux faces opposées ont des
           normales opposées et le critère est pris en valeur absolue : elles
           reçoivent donc toujours le même habillage, ce qui était le défaut à
           corriger (fenêtres d'un côté, mur nu en face).

           Le résultat voyage jusqu'au fragment par le SIGNE de l'UV horizontal,
           négatif pour un mur plein : la structure Vertex est partagée avec le
           terrain et son million de sommets, lui ajouter un attribut pour cette
           seule donnée coûterait 4 Mo par carte. Voir building.frag. */
        float refNx = 1.0f, refNz = 0.0f, refLen2 = -1.0f;
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j = (i + 1) % n;
            const float ex = px[j] - px[i];
            const float ez = pz[j] - pz[i];
            const float len2 = ex * ex + ez * ez;
            if (len2 > refLen2) {
                refLen2 = len2;
                const float inv = 1.0f / std::sqrt(std::max(len2, 1e-6f));
                refNx = ez * inv; /* normale du côté le plus long */
                refNz = -ex * inv;
            }
        }
        /* cos 45 degrés : au-delà, la face regarde dans l'axe de référence. */
        constexpr float COS_45 = 0.70710678f;

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
            const bool fenetree = std::fabs(normal.x * refNx + normal.z * refNz) >= COS_45;
            const float u1 = (fenetree ? 1.0f : -1.0f) * sideLen / FACADE_TILE_W_M;
            pushWallQuad(tb.v, tb.i, bi, bj, tj, ti, normal, wall, u1, vTop);
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
