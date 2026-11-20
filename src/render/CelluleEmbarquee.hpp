/*
 * CelluleEmbarquee.hpp
 * Cellule 3dexport fondue dans l'exécutable des releases. Sa licence interdit
 * de la livrer en fichier lisible : le binaire en porte une copie obscurcie
 * (tools/modele/embarquer_cellule.py), rendue ici à Assimp sous le même nom
 * que le fichier local. Un .glb présent sur le disque garde la priorité.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#pragma once

#include <filesystem>
#include <vector>

namespace artouste::render {

/* Nom du fichier de la cellule, sur le disque ou dans le binaire. */
inline constexpr const char* kNomCelluleEmbarquee = "alouette-3dexport.glb";

/* Vrai si le chemin désigne la cellule et qu'elle est fondue dans le binaire. */
[[nodiscard]] bool celluleEmbarquee(const std::filesystem::path& path);

/* Octets du .glb, désobscurcis ; vide si le binaire n'en porte pas. */
[[nodiscard]] std::vector<unsigned char> lireCelluleEmbarquee();

}  /* namespace artouste::render */
