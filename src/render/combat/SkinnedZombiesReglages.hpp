/*
 * SkinnedZombiesReglages.hpp
 * Réglages du rendu instancié de la horde, partagés entre chargement et dessin.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <cstddef>

namespace artouste::render {

/* Flottants par instance : une mat4 (16, colonnes vec4 aux locations 5-8), un
   flash de coup touché (1, location 9) et une graine de couleur (1, location 10,
   décalage de teinte pour varier les tenues d'un zombie a l'autre). */
constexpr std::size_t FLOATS_PER_INSTANCE = 18;

/* Décorrélation variante/groupe à partir du "kind" du zombie : la variante
   prend les bits de poids faible, le groupe des bits plus hauts (via ce
   diviseur premier), pour que deux zombies voisins ne tombent pas
   systématiquement dans le même lot. */
constexpr unsigned int GROUP_DECORRELATE = 97u;

static_assert(sizeof(SkinnedModel::SkinnedVertex) == 64,
              "disposition du sommet skinné supposée compacte (offsets du VAO)");

} /* namespace artouste::render */
