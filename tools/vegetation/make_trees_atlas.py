#!/usr/bin/env python3
"""
make_trees_atlas.py
Génère un atlas de sprites d'arbres pour la végétation en billboards.
Une seule image RGBA à fond transparent, découpée en N cellules carrées côte à
côte (une espèce par cellule), plaquée sur des billboards EN CROIX (deux quads
perpendiculaires) face au relief. Un arbre est centré dans sa cellule, base en
bas, avec des marges transparentes (pour éviter que le filtrage de texture ne
fasse baver une cellule sur sa voisine).

Trois espèces, choisies dans le moteur selon l'altitude et un tirage aléatoire :
  0 - conifère sombre (sapin), élancé ;
  1 - feuillu arrondi (hêtre / chêne), couronne en boule ;
  2 - mélèze clair, élancé et étroit (étage supérieur).

Réalisme visé (inspiré de FlightGear) : silhouette irrégulière, ombrage interne
(feuillage plus sombre à la base et au coeur, plus clair et lumineux vers le haut
et le côté soleil), et bords doux (léger flou) qui, combinés à l'alpha-to-coverage
du rendu, donnent un feuillage découpé plutôt qu'un aplat aux bords nets.

Le dessin de chaque espèce vit dans species.py, la réduction et l'assemblage
final dans atlas.py (partagé avec compose_trees_atlas.py).

Assets PROCÉDURAUX du prototype (aucune donnée externe, aucune question de licence).
Remplaçables par les atlas FlightGear (Textures/Trees, GPL v2).

Usage : python3 tools/vegetation/make_trees_atlas.py
Sortie : assets/vegetation/trees_atlas.png (768x256, RGBA, 3 cellules de 256)

Auteur : O. Booklage
Licence : GPL v2
"""

import sys
from pathlib import Path

from atlas import assemble_atlas, finish
from species import CELL, SS, broadleaf, conifer, larch

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/
from common.paths import assets_dir


def main():
    trunk_dark = (66, 48, 34)
    cells = [
        finish(conifer((26, 52, 30), (90, 130, 66), trunk_dark, 11), CELL, SS * 0.6),
        finish(broadleaf((44, 82, 38), (120, 158, 78), trunk_dark, 22), CELL, SS * 0.6),
        finish(larch((78, 108, 54), (168, 186, 108), (96, 74, 50), 33), CELL, SS * 0.6),
    ]
    out_path = assets_dir("vegetation", "trees_atlas.png")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    assemble_atlas(cells, out_path)


if __name__ == "__main__":
    main()
