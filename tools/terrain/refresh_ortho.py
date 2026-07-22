#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
refresh_ortho.py
Reemet uniquement l'orthophoto d'une zone deja generee par fetch_terrain.py, a une
resolution eventuellement differente (typiquement plus fine), sans retelecharger le
relief (API altimetrie IGN, couteuse) ni retoucher heightmap.png, buildings.bin ou
les autres fichiers. Met a jour ortho_width / ortho_height dans terrain.txt.

Usage : tools/.venv/bin/python tools/terrain/refresh_ortho.py <zone> [ortho_px]
Exemple (mosaique WMS pour une ortho deux fois plus fine sur dax) :
  tools/.venv/bin/python tools/terrain/refresh_ortho.py dax 10000

Sans ortho_px, reprend la valeur "ortho_px" de la zone (voir terrain/zones/).

Auteur : O. Booklage
Licence : GPL v2
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from terrain import config
from terrain.ortho import fetch_ortho


def main():
    if len(sys.argv) < 2:
        print("usage : refresh_ortho.py <zone> [ortho_px]", file=sys.stderr)
        sys.exit(1)
    zone = sys.argv[1]
    config.select_zone(zone)
    if len(sys.argv) > 2:
        config.ORTHO_HEIGHT = int(sys.argv[2])

    terrain_txt = Path(config.OUT_DIR) / "terrain.txt"
    meta = {}
    for line in terrain_txt.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, _, value = line.partition(" ")
        meta[key] = value.strip()
    width_m, height_m = float(meta["width_m"]), float(meta["height_m"])

    new_width = fetch_ortho(width_m / height_m)

    lines = terrain_txt.read_text().splitlines()
    out = []
    for line in lines:
        if line.startswith("ortho_width "):
            out.append(f"ortho_width {new_width}")
        elif line.startswith("ortho_height "):
            out.append(f"ortho_height {config.ORTHO_HEIGHT}")
        else:
            out.append(line)
    terrain_txt.write_text("\n".join(out) + "\n")
    print(f"[ok] {zone} : ortho {new_width}x{config.ORTHO_HEIGHT} ecrite, terrain.txt mis a jour")


if __name__ == "__main__":
    main()
