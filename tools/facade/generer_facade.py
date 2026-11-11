#!/usr/bin/env python3
"""
generer_facade.py
Génère l'atlas des façades des bâtiments (facade.png) : une colonne par
variante de mur (enduits de maison, béton d'immeuble), trois bandes par
colonne : l'étage courant (fenêtres), le rez-de-chaussée (porte ou vitrine) et
le mur aveugle (pignon). Chaque case est une tuile répétable de
TILE_W_M x TILE_H_M mètres (mêmes valeurs que FACADE_TILE_W_M / FACADE_TILE_H_M
dans src/render/buildings/BuildingsGeometrie.cpp), que building.frag répète
dans sa case ; la variante voyage par l'UV vertical (voir BuildingsGeometrie).

Usage : tools/.venv/bin/python tools/facade/generer_facade.py
Sortie : assets/textures/facade.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/
from common import imaging
from common.paths import assets_dir

# Doit correspondre à FACADE_TILE_W_M / FACADE_TILE_H_M dans BuildingsGeometrie.cpp.
TILE_W_M = 12.0   # largeur réelle de la tuile (3 travées de 4 m)
TILE_H_M = 3.0    # hauteur réelle de la tuile (un étage)
PX_PER_M = 32
WIDTH    = int(TILE_W_M * PX_PER_M)
HEIGHT   = int(TILE_H_M * PX_PER_M)
BAYS     = 3      # travées horizontales

# Une variante par colonne : (enduit, encadrement, bandeau, immeuble, fenêtre,
# volets, porte). La fenêtre est (largeur, hauteur, centre vertical) en fraction
# de la cellule : MÊMES valeurs que FENETRE_DEMI / FENETRE_CENTRE dans
# building.frag, qui y allume les carreaux la nuit. volets : couleur ou None.
# porte : travée de la porte d'entrée. Les trois premières sont des maisons, les
# deux dernières des immeubles (FACADES_MAISON / FACADES_IMMEUBLE dans
# BuildingsGeometrie.hpp).
VARIANTES = [
    # enduit clair, fenêtres hautes à volets gris-bleu
    ((219, 214, 204), (232, 226, 214), (196, 190, 178), False, (0.36, 0.60, 0.55), (96, 112, 128), 1),
    # ocre, fenêtres carrées à volets bois, porte à gauche
    ((214, 190, 150), (236, 228, 212), (190, 166, 128), False, (0.44, 0.46, 0.52), (92, 66, 44), 0),
    # pierre grise, fenêtres étroites sans volets, porte à droite
    ((178, 172, 164), (210, 204, 196), (150, 144, 136), False, (0.28, 0.64, 0.56), None, 2),
    # béton clair, larges baies et balcons
    ((196, 196, 192), (168, 168, 164), (140, 140, 136), True, (0.56, 0.52, 0.55), None, 1),
    # béton beige, baies et balcons
    ((172, 160, 150), (150, 138, 128), (120, 110, 102), True, (0.60, 0.48, 0.55), None, 1),
]
RANGEES = ("etage", "rdc", "plein")   # de haut en bas dans l'image
GLASS_COLORS = [
    (66, 77, 90),
    (58, 68, 82),
    (74, 82, 94),
]  # une teinte par travée : casse la répétition mécanique du carrelage
DOOR_COLOR  = (82, 62, 48)
SHOP_COLOR  = (48, 56, 66)

# Le mur aveugle est le même enduit, assombri : un pignon sans ouverture prend
# moins de lumière du ciel qu'une façade et se distingue ainsi de ses voisines.
PLEIN_GAIN = 0.9


def wall_base(couleur):
    """Fond enduit, avec un grain fin de plâtre (indépendant par pixel, donc
       tuilable sans couture par construction)."""
    base = Image.new("RGB", (WIDTH, HEIGHT), couleur)
    grain = imaging.noise(WIDTH, HEIGHT, 18).point(lambda p: 128 + (p - 128) // 3)
    return ImageChops.overlay(base, Image.merge("RGB", (grain, grain, grain)))


def ombre(img, x0, y0, x1, y1, force=0.35):
    """Ombre portée sous un relief (bandeau, balcon, linteau)."""
    masque = Image.new("L", (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(masque).rectangle([x0, y0, x1, y1], fill=int(255 * force))
    return Image.composite(Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0)), img, masque)


def fenetre(draw, cx, cy, w, h, frame, glass, FRAME):
    x0, x1 = cx - w / 2, cx + w / 2
    y0, y1 = cy - h / 2, cy + h / 2
    draw.rectangle([x0 - frame, y0 - frame, x1 + frame, y1 + frame], fill=FRAME)
    draw.rectangle([x0, y0, x1, y1], fill=glass)
    mullion = max(1, frame // 2)
    mx, my = (x0 + x1) / 2, (y0 + y1) / 2
    draw.rectangle([mx - mullion / 2, y0, mx + mullion / 2, y1], fill=FRAME)
    draw.rectangle([x0, my - mullion / 2, x1, my + mullion / 2], fill=FRAME)
    return (x0, y0, x1, y1)


def facade(variante, rangee):
    """Une case de l'atlas : un étage de la variante, selon la rangée."""
    mur, FRAME, bande, immeuble, (fw, fh, fcy), volets, porte = variante
    img = wall_base(mur)
    draw = ImageDraw.Draw(img)
    bay_w = WIDTH / BAYS
    frame = max(2, int(PX_PER_M * 0.08))

    # Bandeau filant en haut de l'étage (ligne de plancher) et son ombre.
    band_h = max(2, int(HEIGHT * 0.04))
    draw.rectangle([0, 0, WIDTH, band_h], fill=bande)
    img = ombre(img, 0, band_h, WIDTH, band_h * 2, 0.25)
    draw = ImageDraw.Draw(img)

    if rangee == "plein":
        return img.point(lambda p: int(p * PLEIN_GAIN))

    win_w = bay_w * fw
    win_h = HEIGHT * fh
    for col in range(BAYS):
        cx = (col + 0.5) * bay_w
        glass = GLASS_COLORS[col % len(GLASS_COLORS)]
        if rangee == "rdc" and immeuble:
            # Vitrine sur toute la travée, porte vitrée au milieu.
            box = (cx - bay_w * 0.42, HEIGHT * 0.3, cx + bay_w * 0.42, HEIGHT * 0.96)
            draw.rectangle(box, fill=SHOP_COLOR, outline=FRAME, width=frame)
        elif rangee == "rdc" and col == porte:
            # Porte d'entrée, un peu plus haute qu'une fenêtre.
            box = (cx - bay_w * 0.14, HEIGHT * 0.3, cx + bay_w * 0.14, HEIGHT * 0.96)
            draw.rectangle(box, fill=DOOR_COLOR, outline=FRAME, width=frame)
        else:
            box = fenetre(draw, cx, HEIGHT * fcy, win_w, win_h, frame, glass, FRAME)
            if volets:
                # Volets ouverts de part et d'autre, un peu moins hauts que la fenêtre.
                vw = win_w * 0.42
                for sx in (-1, 1):
                    vx = cx + sx * (win_w / 2 + frame + vw / 2 + 1)
                    draw.rectangle([vx - vw / 2, box[1] + 2, vx + vw / 2, box[3] - 2], fill=volets)
                    draw.line([vx, box[1] + 2, vx, box[3] - 2], fill=tuple(int(c * 0.8) for c in volets))
        # Ombre sous le linteau : donne du creux à l'ouverture.
        img = ombre(img, box[0], box[1], box[2], box[1] + frame * 2, 0.4)
        draw = ImageDraw.Draw(img)

    if immeuble and rangee == "etage":
        # Balcon filant : dalle claire en bas de l'étage et son ombre.
        dalle = max(3, int(HEIGHT * 0.06))
        draw.rectangle([0, HEIGHT - dalle, WIDTH, HEIGHT], fill=FRAME)
        img = ombre(img, 0, HEIGHT - dalle * 3, WIDTH, HEIGHT - dalle, 0.45)
    else:
        # Pilastres discrets entre les travées (ombre verticale légère).
        for col in range(1, BAYS):
            x = col * bay_w
            img = ombre(img, x - 1, 0, x + 1, HEIGHT, 0.15)
    return img


def main():
    atlas = Image.new("RGB", (WIDTH * len(VARIANTES), HEIGHT * len(RANGEES)))
    for col, variante in enumerate(VARIANTES):
        for row, rangee in enumerate(RANGEES):
            atlas.paste(facade(variante, rangee), (col * WIDTH, row * HEIGHT))
    sortie = assets_dir("textures", "facade.png")
    sortie.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(sortie)
    print(f"[facade] {sortie} écrit ({atlas.width}x{atlas.height}, "
          f"{len(VARIANTES)} variantes x {len(RANGEES)} rangées, "
          f"case {TILE_W_M:g}x{TILE_H_M:g} m)")


if __name__ == "__main__":
    main()
