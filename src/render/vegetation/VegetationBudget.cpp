/*
 * VegetationBudget.cpp
 * Éclaircissement du semis quand il dépasse le budget d'arbres.
 *
 * Échantillonnage déterministe par indice, sans troncature spatiale : une
 * grande carte perd des arbres partout, pas seulement dans un coin.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/Vegetation.hpp"

#include "render/vegetation/VegetationCouleurs.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace artouste::render {

void Vegetation::eclaircirAuBudget(std::vector<float>& instances, std::size_t count) const {
    /* Budget : au-delà, éclaircissement uniforme (échantillonnage déterministe par
       indice), sans troncature spatiale, pour limiter le surdessin des grandes cartes.
       La valeur vient de la config (clé "arbres_max", défaut TARGET_TREES) ou de la
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
}

} /* namespace artouste::render */
