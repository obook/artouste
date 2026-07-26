#!/usr/bin/env python3
"""
refresh_ortho.py
Réémet uniquement l'orthophoto d'une carte déjà générée, à une résolution
éventuellement différente (typiquement plus fine), sans retélécharger le relief
(API altimétrie IGN, coûteuse) ni retoucher heightmap.png, buildings.bin ou les
autres fichiers. Met à jour ortho_width / ortho_height dans terrain.txt.

Deux sortes de cartes sont acceptées :
  - une zone déclarée dans terrain/zones/ (dax, ossau...) : l'emprise vient de
    sa bbox et, sans argument, ortho_px de la zone sert de résolution ;
  - une carte RECADRÉE depuis une autre (dax-arene, produite par
    crop_zombie_map.py), qui n'a pas d'entrée dans zones/ : l'emprise est lue
    dans son propre terrain.txt et la résolution par défaut est celle déjà en
    place. C'est la seule façon d'affiner l'ortho d'une arène sans rejouer le
    recadrage complet, qui réécrirait aussi heightmap.png.

Usage : tools/.venv/bin/python tools/terrain/refresh_ortho.py <carte> [ortho_px]
Exemples :
  refresh_ortho.py dax 10000         # mosaïque WMS sur la grande carte
  refresh_ortho.py dax-arene 8000    # ~0,25 m/px sur l'arène de 2 km

Auteur : O. Booklage
Licence : GPL v2
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from terrain import config
from terrain.meta import read_meta, update_keys
from terrain.ortho import fetch_ortho
from terrain.zones import ZONES


def main():
    if len(sys.argv) < 2:
        print("usage : refresh_ortho.py <carte> [ortho_px]", file=sys.stderr)
        sys.exit(1)
    carte = sys.argv[1]

    terrain_txt = Path(config.terrain_dir(carte)) / "terrain.txt"
    if not terrain_txt.exists():
        print(f"carte inconnue : {carte} ({terrain_txt} absent)", file=sys.stderr)
        sys.exit(1)
    meta = read_meta(terrain_txt)

    if carte in ZONES:
        config.select_zone(carte)
    else:
        # Carte recadrée : emprise et mer viennent de son terrain.txt, et on
        # repart de sa résolution actuelle faute de clé ortho_px dans zones/.
        config.select_cropped_map(carte, meta)
        config.ORTHO_HEIGHT = int(meta["ortho_height"])
    if len(sys.argv) > 2:
        config.ORTHO_HEIGHT = int(sys.argv[2])

    width_m, height_m = float(meta["width_m"]), float(meta["height_m"])
    new_width = fetch_ortho(width_m / height_m)

    update_keys(terrain_txt, {"ortho_width": new_width, "ortho_height": config.ORTHO_HEIGHT})
    print(f"[ok] {carte} : ortho {new_width}x{config.ORTHO_HEIGHT} écrite, terrain.txt mis à jour")
    print(f"     résolution ~{height_m / config.ORTHO_HEIGHT:.2f} m/px")


if __name__ == "__main__":
    main()
