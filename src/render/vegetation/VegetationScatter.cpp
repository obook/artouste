/*
 * VegetationScatter.cpp
 * Boucle de placement des arbres sur la grille de semis (couleur de forêt,
 * relief, limite forestière, dégagements) et éclaircissement uniforme au
 * budget si le nombre d'arbres dépasse la cible. Extrait de Vegetation.cpp.
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
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace artouste::render {

namespace {

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

}  /* namespace */

std::vector<float> Vegetation::scatterTrees(
    const Terrain& terrain, const unsigned char* ortho, int orthoW, int orthoH, float halfW,
    float halfH, float spacing, bool clear, float sx, float sz,
    const std::vector<unsigned char>& water, const std::vector<unsigned char>& building,
    const std::vector<Exclusion>& exclusions,
    const std::vector<std::pair<float, float>>& fallbackLakes) const {
    /* Un arbre = centre (x, y, z) + largeur + espèce + azimut, empaqueté en six
       flottants pour coller au format d'instance attendu par le shader. */
    std::vector<float> instances;
    const int cols = std::max(1, static_cast<int>((2.0f * halfW) / spacing));
    const int rows = std::max(1, static_cast<int>((2.0f * halfH) / spacing));
    instances.reserve(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));

    std::size_t count = 0;
    for (int r = 0; r < rows && count < MAX_TREES; ++r) {
        for (int c = 0; c < cols && count < MAX_TREES; ++c) {
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

            /* Zones d'exclusion (aérodromes) : aucun arbre dessus. */
            if (!exclusions.empty()) {
                bool excluded = false;
                for (const auto& e : exclusions) {
                    const float dx = x - e.x, dz = z - e.z;
                    if (dx * dx + dz * dz < e.r2) {
                        excluded = true;
                        break;
                    }
                }
                if (excluded) {
                    continue;
                }
            }

            /* Pixel de l'ortho sous le point perturbé (eau et couleur du sol). */
            int ox = 0, oy = 0;
            toPixel(x, z, halfW, halfH, orthoW, orthoH, ox, oy);

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
            orthoRGB(ortho, orthoW, ox, oy, cr, cg, cb);
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
            ++count;
        }
    }

    if (count >= MAX_TREES) {
        std::fprintf(stderr,
                     "[Vegetation] plafond de %zu arbres atteint : semis tronqué au sud. "
                     "Agrandissez ARTOUSTE_TREE_SPACING.\n",
                     MAX_TREES);
    }

    if (count == 0) {
        std::printf("[Vegetation] aucun arbre semé (pas de forêt détectée).\n");
        return instances;
    }

    /* Budget : au-delà, éclaircissement uniforme (échantillonnage déterministe par
       indice), sans troncature spatiale, pour limiter le surdessin des grandes cartes.
       La valeur vient de la config (clé "tree_max", défaut TARGET_TREES) ou de la
       variable d'environnement ARTOUSTE_TREE_MAX, résolue en amont dans initScene ;
       0 signifie "budget par défaut" (sécurité si l'appelant n'en fournit pas). */
    const std::size_t budget = (m_budget > 0) ? m_budget : TARGET_TREES;
    if (count > budget) {
        const float        keep = static_cast<float>(budget) / static_cast<float>(count);
        std::vector<float> thinned;
        thinned.reserve(budget * 6 + 6);
        for (std::size_t i = 0; i < count; ++i) {
            if (unitOf(static_cast<std::uint32_t>(i) ^ 0x2b1f5c3du) < keep) {
                for (int k = 0; k < 6; ++k) {
                    thinned.push_back(instances[i * 6 + static_cast<std::size_t>(k)]);
                }
            }
        }
        std::printf("[Vegetation] semis éclairci : %zu -> %zu arbres (budget).\n", count,
                    thinned.size() / 6);
        instances.swap(thinned);
    }

    return instances;
}

}  /* namespace artouste::render */
