#!/usr/bin/env python3
"""
crop_zombie_map.py
Recadre un terrain Artouste existant (grand, type IGN) en une sous-region legere
autour d'une zone de jeu, pour le mode zombie : on n'a pas besoin de charger toute
la carte de dax pour un combat confine a l'aerodrome.

Le terrain recadre garde EXACTEMENT le meme repere monde grace au decalage
d'origine (cles origin_x / origin_z de terrain.txt, lues par render::Terrain) :
tous les fichiers en coordonnees monde (zombies.txt, buildings.bin, helipads.txt,
exclusions.txt, hapi.txt, landmarks.txt) restent donc valides SANS modification.

Ce qui est reellement allege : heightmap.png et ortho.jpg sont crops a la boite,
et terrain.txt est recalcule. L'ortho (le gros du cout de chargement) passe de
3252x5000 a ~450x450.

Usage : python3 tools/terrain/crop_zombie_map.py <src_dir> <dst_dir> \
            --center-x X --center-z Z --half H
Exemple (aerodrome de Dax-Seyresse, boite ~2 km) :
  python3 tools/terrain/crop_zombie_map.py assets/terrain/dax \
      assets/terrain/dax-arene --center-x 0 --center-z 3492 --half 1000

Boite rectangulaire : --half-x/--half-z remplacent --half independamment (E-O / N-S).

Auteur : O. Booklage - Licence GPL v2
"""

import argparse
import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/ -> paquet terrain
from terrain import config
from terrain.ortho import fetch_ortho


def read_meta(path):
    meta = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        if len(parts) == 2:
            meta[parts[0]] = parts[1].strip()
    return meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src", type=Path)
    ap.add_argument("dst", type=Path)
    ap.add_argument("--center-x", type=float, required=True)
    ap.add_argument("--center-z", type=float, required=True)
    ap.add_argument("--half", type=float, default=1000.0,
                     help="demi-cote de la boite (m), carree ; ignore si --half-x/--half-z "
                          "sont donnes (boite rectangulaire)")
    ap.add_argument("--half-x", type=float, default=None, help="demi-largeur E-O (m)")
    ap.add_argument("--half-z", type=float, default=None, help="demi-hauteur N-S (m)")
    ap.add_argument("--ortho-px", type=int, default=2000,
                     help="hauteur en pixels de l'ortho HD reemise via WMS (defaut 2000, "
                          "~1 m/px sur une boite de 2 km ; limite serveur IGN ~5010)")
    ap.add_argument("--offline-crop", action="store_true",
                     help="ne pas requeter le WMS : recadre l'ortho basse resolution de "
                          "la carte source (comportement historique, hors-ligne mais flou "
                          "de pres une fois au sol)")
    args = ap.parse_args()
    half_x = args.half_x if args.half_x is not None else args.half
    half_z = args.half_z if args.half_z is not None else args.half

    src, dst = args.src, args.dst
    m = read_meta(src / "terrain.txt")
    cols, rows = int(m["cols"]), int(m["rows"])
    width, height = float(m["width_m"]), float(m["height_m"])
    elev_min, elev_max = float(m["elev_min"]), float(m["elev_max"])
    lon_min, lon_max = float(m["lon_min"]), float(m["lon_max"])
    lat_min, lat_max = float(m["lat_min"]), float(m["lat_max"])
    ortho_w, ortho_h = int(m["ortho_width"]), int(m["ortho_height"])

    halfW, halfH = 0.5 * width, 0.5 * height
    dx, dz = width / (cols - 1), height / (rows - 1)

    # --- Plage de colonnes/rangees de la grille couvrant la boite ---------------
    def col_of(x):
        return (x + halfW) / width * (cols - 1)

    def row_of(z):
        return (z + halfH) / height * (rows - 1)

    i_lo = max(0, round(col_of(args.center_x - half_x)))
    i_hi = min(cols - 1, round(col_of(args.center_x + half_x)))
    j_lo = max(0, round(row_of(args.center_z - half_z)))
    j_hi = min(rows - 1, round(row_of(args.center_z + half_z)))
    new_cols, new_rows = i_hi - i_lo + 1, j_hi - j_lo + 1

    # Etendue monde reelle de la grille cropee (bornes des colonnes/rangees).
    def x_at(i):
        return -halfW + i * dx

    def z_at(j):
        return -halfH + j * dz

    x0, x1 = x_at(i_lo), x_at(i_hi)
    z0, z1 = z_at(j_lo), z_at(j_hi)
    new_width = x1 - x0
    new_height = z1 - z0
    origin_x = 0.5 * (x0 + x1)  # centre de la boite en coord MONDE
    origin_z = 0.5 * (z0 + z1)

    # --- Nouvelles bornes lon/lat (memes formules que render::Terrain) ----------
    def lon_of(x):
        return lon_min + (x / width + 0.5) * (lon_max - lon_min)

    def lat_of(z):
        return lat_max - (z / height + 0.5) * (lat_max - lat_min)

    new_lon_min, new_lon_max = lon_of(x0), lon_of(x1)
    new_lat_max, new_lat_min = lat_of(z0), lat_of(z1)  # z0=nord -> lat max

    dst.mkdir(parents=True, exist_ok=True)

    # --- Heightmap 16 bits ------------------------------------------------------
    hm = Image.open(src / "heightmap.png")
    arr = np.array(hm)  # (rows, cols), uint16
    if arr.dtype != np.uint16:
        arr = arr.astype(np.uint16)
    sub = arr[j_lo:j_hi + 1, i_lo:i_hi + 1]
    Image.fromarray(sub, mode="I;16").save(dst / "heightmap.png")

    # --- Ortho -------------------------------------------------------------------
    # Par defaut, on ne recadre PAS l'ortho basse resolution de la carte source :
    # celle-ci partage son budget de pixels WMS (limite serveur IGN ~5010 px) avec
    # toute l'emprise de la grande carte (des dizaines de km), ce qui donne un sol
    # flou vu de pres une fois recadre sur une arene de ~2 km. On reemet donc une
    # requete WMS dediee, centree sur la seule boite de l'arene : le meme budget de
    # pixels serveur, applique a une emprise bien plus petite, donne une resolution
    # nettement meilleure (voir --ortho-px).
    if args.offline_crop:
        def ox_of(x):
            return (x + halfW) / width * ortho_w

        def oy_of(z):
            return (z + halfH) / height * ortho_h

        ox_lo, ox_hi = round(ox_of(x0)), round(ox_of(x1))
        oy_lo, oy_hi = round(oy_of(z0)), round(oy_of(z1))  # z0=nord -> ligne haute
        ortho = Image.open(src / "ortho.jpg").convert("RGB")
        ortho.crop((ox_lo, oy_lo, ox_hi, oy_hi)).save(dst / "ortho.jpg", quality=92)
        new_ortho_w, new_ortho_h = ox_hi - ox_lo, oy_hi - oy_lo
    else:
        config.LON_MIN, config.LON_MAX = new_lon_min, new_lon_max
        config.LAT_MIN, config.LAT_MAX = new_lat_min, new_lat_max
        config.RECOLOR_SEA = m.get("sea", "0") == "1"
        config.ORTHO_HEIGHT = args.ortho_px
        config.OUT_DIR = str(dst)
        new_ortho_w = fetch_ortho(new_width / new_height)
        new_ortho_h = args.ortho_px

    # --- terrain.txt recadre ----------------------------------------------------
    start_x = m.get("start_x", "0")
    start_z = m.get("start_z", "0")
    start_heading = m.get("start_heading", "0")
    sea = m.get("sea", "0")
    lines = [
        f"# Terrain Artouste - recadre depuis {src.name} (mode zombie, sous-region)",
        "# Repere monde identique a la carte source (origin_x/origin_z) : tous les",
        "# fichiers en coordonnees monde restent valides sans decalage.",
        f"cols {new_cols}",
        f"rows {new_rows}",
        f"width_m {new_width:.1f}",
        f"height_m {new_height:.1f}",
        f"elev_min {elev_min:.2f}",
        f"elev_max {elev_max:.2f}",
        f"lon_min {new_lon_min:.6f}",
        f"lon_max {new_lon_max:.6f}",
        f"lat_min {new_lat_min:.6f}",
        f"lat_max {new_lat_max:.6f}",
        f"ortho_width {new_ortho_w}",
        f"ortho_height {new_ortho_h}",
        f"sea {sea}",
        f"start_x {start_x}",
        f"start_z {start_z}",
        f"start_heading {start_heading}",
        f"origin_x {origin_x:.2f}",
        f"origin_z {origin_z:.2f}",
    ]
    (dst / "terrain.txt").write_text("\n".join(lines) + "\n")

    # --- Fichiers annexes copies tels quels (coordonnees monde / lon-lat) --------
    for aux in ["zombies.txt", "buildings.bin", "helipads.txt", "exclusions.txt",
                "hapi.txt", "landmarks.txt"]:
        p = src / aux
        if p.exists():
            shutil.copy2(p, dst / aux)

    print(f"[crop] {src.name} -> {dst.name}")
    print(f"  grille {new_cols}x{new_rows} ({new_width:.0f}x{new_height:.0f} m), "
          f"ortho {new_ortho_w}x{new_ortho_h}")
    print(f"  origin monde ({origin_x:.1f}, {origin_z:.1f})")


if __name__ == "__main__":
    main()
