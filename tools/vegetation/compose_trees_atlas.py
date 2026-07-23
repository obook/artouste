#!/usr/bin/env python3
"""
compose_trees_atlas.py
Compose l'atlas de sprites d'arbres (assets/vegetation/trees_atlas.png) à partir
des cellules photographiques extraites des atlas FlightGear (rangées dans
assets/vegetation/fgdata-trees/, licence GPL v2 -- voir CREDITS.txt).

Chaque espèce est détourée (rognée sur son alpha), mise à l'échelle pour tenir
dans une cellule de 256x512, et calée sur la BASE (bas de la cellule) et centrée
horizontalement -- le pied de l'arbre touche ainsi le sol dans le moteur. Les
trois cellules sont posées côte à côte : atlas final 768x512, agencement attendu
par le shader (ATLAS_COUNT = 3, une colonne par espèce).

Ordre des espèces (doit coller au choix d'espèce dans render::Vegetation) :
  0 sapin (conifère sombre), 1 feuillu, 2 mélèze / épicéa (conifère clair).

L'assemblage final (collage côte à côte, écriture) vit dans atlas.py, partagé
avec make_trees_atlas.py (dont les cellules sont procédurales, pas photographiques).

Usage : python3 tools/vegetation/compose_trees_atlas.py
Sortie : assets/vegetation/trees_atlas.png (768x512, RGBA)

Auteur : O. Booklage
Licence : GPL v2 (assets sources : FlightGear, GPL v2)
"""

import sys
from pathlib import Path

from PIL import Image

from atlas import assemble_atlas

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/
from common.paths import assets_dir

CELL_W = 256
CELL_H = 512
FILL_H = 0.98   # fraction de la hauteur de cellule que l'arbre peut occuper au plus


def fit_cell(src):
    """Détoure, met à l'échelle pour tenir dans (CELL_W, CELL_H*FILL_H), et cale sur
    la base (bas) de la cellule, centré horizontalement."""
    bbox = src.getbbox()            # boîte englobante des pixels non transparents
    if bbox is not None:
        src = src.crop(bbox)
    w, h = src.size
    scale = min(CELL_W / w, (CELL_H * FILL_H) / h)
    nw, nh = max(1, int(round(w * scale))), max(1, int(round(h * scale)))
    src = src.resize((nw, nh), Image.LANCZOS)

    cell = Image.new("RGBA", (CELL_W, CELL_H), (0, 0, 0, 0))
    x = (CELL_W - nw) // 2          # centré horizontalement
    y = CELL_H - nh                 # calé sur la base (bas de la cellule)
    cell.paste(src, (x, y), src)
    return cell


def main():
    veg = assets_dir("vegetation")
    src_dir = veg / "fgdata-trees"

    order = ["conifer_fir.png", "broadleaf.png", "conifer_spruce.png"]  # 0,1,2
    cells = [fit_cell(Image.open(src_dir / name).convert("RGBA")) for name in order]

    out_path = veg / "trees_atlas.png"
    assemble_atlas(cells, out_path, verbe="composé", note="sources FlightGear GPL v2")


if __name__ == "__main__":
    main()
