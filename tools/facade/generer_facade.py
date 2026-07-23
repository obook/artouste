#!/usr/bin/env python3
"""
generer_facade.py
Génère la texture de façade tuilable des bâtiments : mur enduit clair, percé de
fenêtres sur deux étages et trois travées, avec un grain de plâtre discret.
Échantillonnée par building.frag (u_facade) en répétition (GL_REPEAT), sur une
tuile réelle de TILE_W_M x TILE_H_M mètres (mêmes valeurs que
FACADE_TILE_W_M / FACADE_TILE_H_M dans src/render/Buildings.cpp) : Buildings.cpp
calcule les coordonnées UV des murs en mètres réels, donc la tuile se pose à la
même échelle sur un petit pavillon et sur un grand immeuble.

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

# Doit correspondre à FACADE_TILE_W_M / FACADE_TILE_H_M dans Buildings.cpp.
TILE_W_M = 12.0   # largeur réelle de la tuile (3 travées de 4 m)
TILE_H_M = 6.0    # hauteur réelle de la tuile (2 étages de 3 m)
PX_PER_M = 32
WIDTH    = int(TILE_W_M * PX_PER_M)
HEIGHT   = int(TILE_H_M * PX_PER_M)
BAYS     = 3      # travées horizontales
FLOORS   = 2      # étages verticaux

WALL_COLOR  = (219, 214, 204)     # enduit clair, assorti à WALL_COLOR (Buildings.cpp)
FRAME_COLOR = (232, 226, 214)     # encadrement des fenêtres, un peu plus clair
BAND_COLOR  = (196, 190, 178)     # bandeau filant entre les étages
GLASS_COLORS = [
    (66, 77, 90),
    (58, 68, 82),
    (74, 82, 94),
]  # une teinte par travée : casse la répétition mécanique du carrelage


def wall_base():
    """Fond enduit clair, avec un grain fin de plâtre (indépendant par pixel,
       donc tuilable sans couture par construction : pas de corrélation spatiale
       à casser au bord)."""
    base = Image.new("RGB", (WIDTH, HEIGHT), WALL_COLOR)
    grain = imaging.noise(WIDTH, HEIGHT, 18).point(lambda p: 128 + (p - 128) // 3)
    grain_rgb = Image.merge("RGB", (grain, grain, grain))
    return ImageChops.overlay(base, grain_rgb)


def main():
    img = wall_base()
    draw = ImageDraw.Draw(img)

    bay_w   = WIDTH / BAYS
    floor_h = HEIGHT / FLOORS

    # Bandeau filant entre les deux étages (ligne de plancher).
    band_h = max(2, int(HEIGHT * 0.02))
    draw.rectangle([0, floor_h - band_h / 2, WIDTH, floor_h + band_h / 2], fill=BAND_COLOR)

    # Une fenêtre par travée et par étage, centrée dans sa cellule.
    win_w = bay_w * 0.56
    win_h = floor_h * 0.52
    frame = max(2, int(PX_PER_M * 0.08))
    for col in range(BAYS):
        cx = (col + 0.5) * bay_w
        glass = GLASS_COLORS[col % len(GLASS_COLORS)]
        for row in range(FLOORS):
            cy = (row + 0.5) * floor_h
            x0, x1 = cx - win_w / 2, cx + win_w / 2
            y0, y1 = cy - win_h / 2, cy + win_h / 2
            draw.rectangle([x0 - frame, y0 - frame, x1 + frame, y1 + frame], fill=FRAME_COLOR)
            draw.rectangle([x0, y0, x1, y1], fill=glass)
            # Croisillon : partage la fenêtre en quatre carreaux.
            mullion = max(1, frame // 2)
            mx, my = (x0 + x1) / 2, (y0 + y1) / 2
            draw.rectangle([mx - mullion / 2, y0, mx + mullion / 2, y1], fill=FRAME_COLOR)
            draw.rectangle([x0, my - mullion / 2, x1, my + mullion / 2], fill=FRAME_COLOR)

    # Pilastres discrets entre les travées (ombre verticale légère).
    for col in range(1, BAYS):
        x = col * bay_w
        pilaster = Image.new("L", (WIDTH, HEIGHT), 0)
        ImageDraw.Draw(pilaster).line(
            [x, 0, x, HEIGHT], fill=40, width=max(2, int(PX_PER_M * 0.05)))
        shade = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
        img = Image.composite(shade, img, pilaster.point(lambda p: int(p * 0.35)))

    sortie = assets_dir("textures", "facade.png")
    sortie.parent.mkdir(parents=True, exist_ok=True)
    img.save(sortie)
    print(f"[facade] {sortie} écrit ({WIDTH}x{HEIGHT}, tuile {TILE_W_M:g}x{TILE_H_M:g} m)")


if __name__ == "__main__":
    main()
