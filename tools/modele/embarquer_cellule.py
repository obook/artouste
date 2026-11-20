#!/usr/bin/env python3
"""
tools/modele/embarquer_cellule.py
Écrit le .glb de la cellule 3dexport dans un source C++, obscurci, pour le
fondre dans l'exécutable des releases (voir docs/ASSETS.md).

La licence Basic de 3dexport interdit de livrer le modèle sous une forme que
le joueur peut récupérer tel quel : ni dans le dépôt, ni en fichier dans
l'archive. Le .glb voyage donc dans le binaire, octets mélangés à une clé.

Usage : embarquer_cellule.py <entrée.glb> <sortie.cpp>
Appelé par CMake quand ARTOUSTE_CELLULE_EMBARQUEE désigne un .glb.

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

# Doit rester identique à kCle dans src/render/CelluleEmbarquee.cpp.
CLE = b"Artouste-Alouette-II-SA313B"


def main() -> None:
    entree, sortie = sys.argv[1], sys.argv[2]
    with open(entree, "rb") as f:
        data = bytearray(f.read())
    if data[:4] != b"glTF":
        sys.exit(f"embarquer_cellule : {entree} n'est pas un .glb")
    taille = len(data)
    for i in range(taille):
        data[i] ^= CLE[i % len(CLE)] ^ (i & 0xFF)

    # Mots de 64 bits : huit fois moins d'éléments à analyser pour le
    # compilateur qu'un tableau d'octets (13 Mo de modèle).
    data += bytes(-taille % 8)
    mots = (str(int.from_bytes(data[i:i + 8], "little")) + "u"
            for i in range(0, len(data), 8))

    with open(sortie, "w", encoding="utf-8") as f:
        f.write("// Généré par tools/modele/embarquer_cellule.py, ne pas modifier.\n")
        f.write("#include <cstddef>\n#include <cstdint>\n\n")
        f.write("namespace artouste::render::cellule {\n")
        f.write(f"extern const std::size_t kTaille = {taille};\n")
        f.write("extern const std::uint64_t kMots[] = {\n")
        ligne = []
        for mot in mots:
            ligne.append(mot)
            if len(ligne) == 8:
                f.write(",".join(ligne) + ",\n")
                ligne = []
        if ligne:
            f.write(",".join(ligne) + ",\n")
        f.write("};\n}\n")


if __name__ == "__main__":
    main()
