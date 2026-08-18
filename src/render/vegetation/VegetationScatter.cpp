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

#include "render/vegetation/VegetationCouleurs.hpp"

#include "render/Terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace artouste::render {



std::vector<float> Vegetation::scatterTrees(
    const Terrain& terrain, const unsigned char* ortho, int orthoW, int orthoH, float halfW,
    float halfH, float spacing, bool clear, float sx, float sz,
    const std::vector<unsigned char>& water, const std::vector<unsigned char>& building,
    const std::vector<unsigned char>& forest, int forestW, int forestH,
    const std::vector<Exclusion>& exclusions,
    const std::vector<std::pair<float, float>>& fallbackLakes) const {
    /* Un arbre = centre (x, y, z) + largeur + espèce + azimut, empaqueté en six
       flottants pour coller au format d'instance attendu par le shader. */
    std::vector<float> instances;
    const int cols = std::max(1, static_cast<int>((2.0f * halfW) / spacing));
    const int rows = std::max(1, static_cast<int>((2.0f * halfH) / spacing));
    instances.reserve(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));

    /* Couleur du sol moyennée sur le pixel et ses quatre voisins : une orthophoto
       compressée en JPEG a des pixels isolés aberrants, et un seul d'entre eux ne
       doit pas décider du sort d'un arbre. */
    const auto couleurDuSol = [&](int ox, int oy, float& r, float& g, float& b) {
        constexpr int dx[5] = {0, -1, 1, 0, 0};
        constexpr int dy[5] = {0, 0, 0, -1, 1};
        r = g = b = 0.0f;
        for (int k = 0; k < 5; ++k) {
            float cr = 0.0f, cg = 0.0f, cb = 0.0f;
            orthoRGB(ortho, orthoW, std::clamp(ox + dx[k], 0, orthoW - 1),
                     std::clamp(oy + dy[k], 0, orthoH - 1), cr, cg, cb);
            r += cr;
            g += cg;
            b += cb;
        }
        r *= 0.2f;
        g *= 0.2f;
        b *= 0.2f;
    };

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

            /* Forêt cartographiée (masque IGN) sous le point, si la carte a son
               masque. Elle tranche mieux que la couleur : celle-ci plante sur une
               pelouse de stade, un hippodrome ou une ombre de versant, et rate une
               forêt en plein soleil ou en couleurs d'automne. En France elle fait
               autorité (rien d'autre n'est boisé) ; on ne retombe sur la couleur
               qu'à l'étranger, où elle ne dit rien (versant espagnol d'une carte
               frontalière). */
            unsigned char classeForet = ForestHorsFrance;
            if (!forest.empty()) {
                int fx = 0, fy = 0;
                toPixel(x, z, halfW, halfH, forestW, forestH, fx, fy);
                classeForet = forest[static_cast<std::size_t>(fy)
                                         * static_cast<std::size_t>(forestW)
                                     + static_cast<std::size_t>(fx)];
            }
            if (classeForet == ForestNonBoise) {
                continue;  /* en France, non boisé : aucun arbre */
            }
            float cr = 0.0f, cg = 0.0f, cb = 0.0f;
            couleurDuSol(ox, oy, cr, cg, cb);
            if (classeForet == ForestHorsFrance) {
                /* Couleur du sol : on ne plante que sur du vert de forêt. */
                if (!looksLikeForest(cr, cg, cb)) {
                    continue;
                }
            } else if (looksMineral(cr, cg, cb)) {
                continue;  /* forêt cartographiée, mais sol nu ici : coupe rase,
                              pare-feu, piste forestière, toiture sous couvert */
            }

            /* Altitude : posé sur le relief. Au-dessus de la limite forestière, le
               couvert se raréfie progressivement (transition forêt -> pelouse) plutôt
               que de s'arrêter net ; sous le niveau de la mer, rien (terrains côtiers).
               heightAt() attend des coordonnées MONDE (décalées de originX/originZ sur
               une carte recadrée), alors que x,z ici sont LOCALES : sans la conversion,
               heightAt() se croit hors emprise et renvoie 0, plantant les arbres au
               niveau de la mer au lieu du vrai relief (arbres flottants ou enterrés). */
            const float y = terrain.heightAt(x + terrain.originX(), z + terrain.originZ());
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

            /* Espèce : l'essence dominante de la BD Forêt quand on l'a, sinon
               l'altitude comme avant (résineux de plus en plus fréquent en
               montant). Quatre planches à l'atlas : sapin (0), feuillu (1),
               mélèze (2), pin (3). Les résineux autres que le pin se répartissent
               entre sapin et mélèze selon l'étage. */
            const float t  = std::clamp((y - 1000.0f) / 700.0f, 0.0f, 1.0f);
            const float r1 = unitOf(seed ^ 0x27d4eb2fu);
            const float r2 = unitOf(seed ^ 0x165667b1u);
            float       species = 1.0f;  /* feuillu */
            bool        conifere = false;
            switch (classeForet) {
                case ForestFeuillu:  conifere = false; break;
                case ForestPin:      species = 3.0f;   break;
                case ForestConifere: conifere = true;  break;
                case ForestMixte:    conifere = (r1 < 0.5f); break;
                default:             conifere = (r1 < 0.30f + 0.45f * t); break;
            }
            if (conifere) {
                /* Mélèze seulement à l'étage supérieur, et minoritaire face au sapin. */
                species = (t > 0.5f && r2 < 0.35f) ? 2.0f : 0.0f;
            }

            /* Azimut de la croix : oriente les deux quads perpendiculaires, décorrélé
               d'un arbre à l'autre pour éviter des rangées alignées. */
            const float azimuth = unitOf(seed ^ 0x68bc21ebu) * 6.2831853f;

            /* Position finale envoyée au GPU (a_center du shader) : x,z sont ici en
               coordonnées LOCALES (centrées sur 0, pour le calcul ci-dessus), mais le
               shader attend du monde, comme le terrain et les bâtiments (u_model est
               le même recalage caméra pour tous). Sans le décalage d'origine, tout le
               semis se retrouve translaté hors du terrain réellement affiché sur une
               carte recadrée. */
            instances.push_back(x + terrain.originX());
            instances.push_back(y);
            instances.push_back(z + terrain.originZ());
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

    eclaircirAuBudget(instances, count);
    return instances;
}

}  /* namespace artouste::render */
