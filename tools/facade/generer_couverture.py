#!/usr/bin/env python3
"""
generer_couverture.py
Génère la texture de couverture des toits (couverture.png), en niveaux de gris
centrés sur 128 : building.frag la multiplie par la couleur du toit (tirée de
l'orthophoto), elle ne donne que le relief. Deux cases superposées, chacune
répétable sur TILE_M mètres : en haut les rangs de tuiles ou d'ardoises des
toits en pente, en bas le gravier des toits-terrasses.

Usage : tools/.venv/bin/python tools/facade/generer_couverture.py
Sortie : assets/textures/couverture.png

Auteur : O. Booklage
Licence : GPL v2
"""

import random
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/
from common import imaging
from common.paths import assets_dir

TILE_M   = 2.0    # taille réelle d'une case (même valeur : COUVERTURE_TILE_M, BuildingsGeometrie.cpp)
PX_PER_M = 64
SIZE     = int(TILE_M * PX_PER_M)
RANG_M   = 0.33   # hauteur d'un rang de tuiles (ou d'ardoises)
TUILE_M  = 0.25   # largeur d'une tuile


def rangs():
    """Rangs de tuiles en quinconce : chaque tuile un peu plus claire ou plus
       sombre, le bas de chaque rang dans l'ombre du rang suivant."""
    random.seed(7)
    img = Image.new("L", (SIZE, SIZE), 128)
    draw = ImageDraw.Draw(img)
    rang_px = SIZE / round(TILE_M / RANG_M)      # un nombre entier de rangs : tuilable
    tuile_px = SIZE / round(TILE_M / TUILE_M)
    nb_rangs = int(round(SIZE / rang_px))
    nb_tuiles = int(round(SIZE / tuile_px))
    for r in range(nb_rangs):
        y0 = r * rang_px
        decale = (r % 2) * tuile_px / 2
        for t in range(-1, nb_tuiles + 1):
            x0 = t * tuile_px + decale
            teinte = 128 + random.randint(-14, 14)
            draw.rectangle([x0, y0, x0 + tuile_px - 1, y0 + rang_px - 1], fill=teinte)
            # Joint vertical, plus sombre.
            draw.line([x0, y0, x0, y0 + rang_px - 1], fill=teinte - 30)
        # Ombre portée du rang du dessus sur le haut de celui-ci.
        ombre = max(2, int(rang_px * 0.22))
        for k in range(ombre):
            draw.line([0, y0 + k, SIZE, y0 + k], fill=None)
            ligne = img.crop((0, int(y0 + k), SIZE, int(y0 + k) + 1))
            ligne = ligne.point(lambda p, f=(1 - k / ombre): int(p * (1 - 0.35 * f)))
            img.paste(ligne, (0, int(y0 + k)))
    grain = imaging.noise(SIZE, SIZE, 20).point(lambda p: 128 + (p - 128) // 4)
    return ImageChops.overlay(img, grain)


def gravier():
    """Gravillons : deux grains superposés, l'un fin, l'autre flouté."""
    fin = imaging.noise(SIZE, SIZE, 40)
    gros = imaging.noise(SIZE, SIZE, 60, blur=1.2)
    img = ImageChops.overlay(fin, gros).filter(ImageFilter.SMOOTH)
    return img.point(lambda p: 128 + (p - 128) // 2)


def main():
    atlas = Image.new("L", (SIZE, SIZE * 2))
    atlas.paste(rangs(), (0, 0))
    atlas.paste(gravier(), (0, SIZE))
    sortie = assets_dir("textures", "couverture.png")
    atlas.save(sortie)
    print(f"[couverture] {sortie} écrit ({atlas.width}x{atlas.height}, case {TILE_M:g} m)")


if __name__ == "__main__":
    main()
