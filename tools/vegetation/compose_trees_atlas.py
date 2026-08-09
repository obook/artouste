#!/usr/bin/env python3
"""
compose_trees_atlas.py
Compose l'atlas de sprites d'arbres (assets/vegetation/trees_atlas.png) à partir
des cellules photographiques extraites des atlas FlightGear (rangées dans
assets/vegetation/fgdata-trees/, licence GPL v2 -- voir CREDITS.txt).

Chaque espèce est détourée (rognée sur son alpha), mise à l'échelle pour tenir
dans une cellule de 256x512, et calée sur la BASE (bas de la cellule) et centrée
horizontalement -- le pied de l'arbre touche ainsi le sol dans le moteur. Les
quatre cellules sont posées côte à côte : atlas final 1024x512, agencement attendu
par le shader (ATLAS_COUNT = 4, une colonne par espèce).

Ordre des espèces (doit coller au choix d'espèce dans render::Vegetation) :
  0 sapin (conifère sombre), 1 feuillu, 2 mélèze / épicéa (conifère clair),
  3 pin (verticilles de longues aiguilles) -- le pin maritime des Landes et le
  pin sylvestre sont l'essence dominante de plusieurs cartes, la BD Forêt les
  distingue, l'atlas doit donc savoir les montrer. Le sujet FlightGear est un
  jeune arbre au tronc court : elancer() lui allonge le fût pour retrouver la
  silhouette d'un pin adulte de pignada (houppier haut perché sur un long fût nu).

L'assemblage final (collage côte à côte, écriture) vit dans atlas.py, partagé
avec make_trees_atlas.py (dont les cellules sont procédurales, pas photographiques).

Usage : python3 tools/vegetation/compose_trees_atlas.py
Sortie : assets/vegetation/trees_atlas.png (1024x512, RGBA)

Auteur : O. Booklage
Licence : GPL v2 (assets sources : FlightGear, GPL v2)
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image

from atlas import assemble_atlas

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/
from common.paths import assets_dir

CELL_W = 256
CELL_H = 512
FILL_H = 0.98   # fraction de la hauteur de cellule que l'arbre peut occuper au plus
FUT_PIN = 0.55  # part de la hauteur du pin occupée par le fût nu (voir elancer)


def elancer(src, part_fut):
    """Allonge le fût d'un arbre : garde le houppier tel quel et étire la portion
    de tronc nu du bas jusqu'à ce qu'elle occupe part_fut de la hauteur totale.
    Le fût nu est repéré par la largeur des rangées opaques : sous le houppier,
    l'arbre se réduit au tronc. Sert au pin, dont le sujet FlightGear est un jeune
    arbre trapu là où le pin maritime adulte porte son houppier très haut."""
    src = src.crop(src.getbbox())
    largeur = (np.asarray(src)[..., 3] > 40).sum(axis=1)
    seuil = 0.20 * largeur.max()
    y = src.height - 1
    while y > 0 and largeur[y] <= seuil:
        y -= 1
    haut_fut = y + 1
    if haut_fut >= src.height - 2:
        return src                       # pas de fût nu identifiable : inchangé
    houppier = src.crop((0, 0, src.width, haut_fut))
    fut = src.crop((0, haut_fut, src.width, src.height))
    cible = int(round(houppier.height / (1.0 - part_fut) * part_fut))
    fut = fut.resize((fut.width, max(fut.height, cible)), Image.LANCZOS)
    out = Image.new("RGBA", (src.width, houppier.height + fut.height), (0, 0, 0, 0))
    out.paste(houppier, (0, 0))
    out.paste(fut, (0, houppier.height))
    return out


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

    order = ["conifer_fir.png", "broadleaf.png", "conifer_spruce.png",
             "pine.png"]  # 0,1,2,3
    sources = [Image.open(src_dir / name).convert("RGBA") for name in order]
    sources[3] = elancer(sources[3], FUT_PIN)
    cells = [fit_cell(src) for src in sources]

    out_path = veg / "trees_atlas.png"
    assemble_atlas(cells, out_path, verbe="composé", note="sources FlightGear GPL v2")


if __name__ == "__main__":
    main()
