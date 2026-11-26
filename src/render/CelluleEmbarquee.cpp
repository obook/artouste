/*
 * CelluleEmbarquee.cpp
 * Désobscurcit la cellule 3dexport fondue dans le binaire. Les données et
 * leur clé (kTaille, kMots, kCle) viennent d'un source généré à la compilation quand
 * ARTOUSTE_CELLULE_EMBARQUEE est défini ; sinon ARTOUSTE_SANS_CELLULE est posé
 * et rien n'est embarqué.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#include "render/CelluleEmbarquee.hpp"

#include <cstddef>
#include <cstdint>

#ifndef ARTOUSTE_SANS_CELLULE
namespace artouste::render::cellule {
extern const std::size_t   kTaille;
extern const std::uint64_t kMots[];
extern const std::size_t   kTailleCle;
extern const unsigned char kCle[];
}  /* namespace artouste::render::cellule */
#endif

namespace artouste::render {

/* shortcut: simple mélange, pas un chiffrement, et la clé est dans le binaire ;
   il suffit que le .glb ne soit pas lisible tel quel, comme les .pak des jeux. */

bool celluleEmbarquee(const std::filesystem::path& path) {
#ifdef ARTOUSTE_SANS_CELLULE
    (void)path;
    return false;
#else
    return path.filename() == kNomCelluleEmbarquee;
#endif
}

std::vector<unsigned char> lireCelluleEmbarquee() {
    std::vector<unsigned char> data;
#ifndef ARTOUSTE_SANS_CELLULE
    data.resize(cellule::kTaille);
    for (std::size_t i = 0; i < data.size(); ++i) {
        const auto octet = static_cast<unsigned char>(cellule::kMots[i / 8] >> (8 * (i % 8)));
        data[i] = static_cast<unsigned char>(
            octet ^ cellule::kCle[i % cellule::kTailleCle] ^ (i & 0xFFu));
    }
#endif
    return data;
}

}  /* namespace artouste::render */
