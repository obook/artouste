#!/usr/bin/env python3
"""
crop_zombie_map.py
Recadre un terrain Artouste existant (grand, type IGN) en une sous-région légère
autour d'une zone de jeu, pour le mode zombie : on n'a pas besoin de charger toute
la carte de Dax pour un combat confiné à l'aérodrome.

Le terrain recadré garde EXACTEMENT le même repère monde grâce au décalage
d'origine (clés origin_x / origin_z de terrain.txt, lues par render::Terrain) :
tous les fichiers en coordonnées monde (zombies.txt, buildings.bin, helipads.txt,
exclusions.txt, hapi.txt, landmarks.txt) restent donc valides SANS modification.

Ce qui est réellement allégé : heightmap.png et ortho.jpg sont recadrés à la
boîte, et terrain.txt est recalculé. L'ortho (le gros du coût de chargement)
passe de 3252x5000 à ~450x450.

Usage : python3 tools/terrain/crop_zombie_map.py <src_dir> <dst_dir> \
            --center-x X --center-z Z --half H
Exemple (aérodrome de Dax-Seyresse, boîte ~2 km) :
  python3 tools/terrain/crop_zombie_map.py assets/terrain/dax \
      assets/terrain/dax-arene --center-x 0 --center-z 3492 --half 1000

Boîte rectangulaire : --half-x/--half-z remplacent --half indépendamment (E-O / N-S).

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
from terrain.grid import Grid
from terrain.meta import read_meta
from terrain.ortho import fetch_ortho

# Fichiers annexes en coordonnées monde ou lon/lat : copiés tels quels, sans
# modification (voir le repère monde partagé, ci-dessus). forest.png n'en est
# PAS : c'est une image calée sur l'emprise de la carte source, la copier la
# décalerait. On la refait pour la carte recadrée avec
# tools/fetch_forest.py <carte recadrée>, qui lit son propre terrain.txt.
FICHIERS_ANNEXES = ["zombies.txt", "buildings.bin", "helipads.txt", "exclusions.txt",
                    "hapi.txt", "landmarks.txt"]


def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument("src", type=Path)
    ap.add_argument("dst", type=Path)
    ap.add_argument("--center-x", type=float, required=True)
    ap.add_argument("--center-z", type=float, required=True)
    ap.add_argument("--half", type=float, default=1000.0,
                     help="demi-côté de la boîte (m), carrée ; ignoré si --half-x/--half-z "
                          "sont donnés (boîte rectangulaire)")
    ap.add_argument("--half-x", type=float, default=None, help="demi-largeur E-O (m)")
    ap.add_argument("--half-z", type=float, default=None, help="demi-hauteur N-S (m)")
    ap.add_argument("--ortho-px", type=int, default=2000,
                     help="hauteur en pixels de l'ortho HD réémise via WMS (défaut 2000, "
                          "~1 m/px sur une boîte de 2 km ; limite serveur IGN ~5010)")
    ap.add_argument("--offline-crop", action="store_true",
                     help="ne pas requêter le WMS : recadre l'ortho basse résolution de "
                          "la carte source (comportement historique, hors ligne mais flou "
                          "de près une fois au sol)")
    return ap.parse_args()


def crop_bounds(grid, args):
    """Indices de grille [i_lo, i_hi] x [j_lo, j_hi] couvrant la boîte centrée
       sur (center_x, center_z), bornés à la grille source."""
    half_x = args.half_x if args.half_x is not None else args.half
    half_z = args.half_z if args.half_z is not None else args.half
    i_lo = max(0, round(grid.col_of(args.center_x - half_x)))
    i_hi = min(grid.cols - 1, round(grid.col_of(args.center_x + half_x)))
    j_lo = max(0, round(grid.row_of(args.center_z - half_z)))
    j_hi = min(grid.rows - 1, round(grid.row_of(args.center_z + half_z)))
    return i_lo, i_hi, j_lo, j_hi


def crop_heightmap(src, dst, i_lo, i_hi, j_lo, j_hi):
    """Recadre heightmap.png (16 bits) sur les indices de grille donnés."""
    hm = Image.open(src / "heightmap.png")
    arr = np.array(hm)
    if arr.dtype != np.uint16:
        arr = arr.astype(np.uint16)
    sub = arr[j_lo:j_hi + 1, i_lo:i_hi + 1]
    Image.fromarray(sub, mode="I;16").save(dst / "heightmap.png")


def produce_ortho(args, grid, src, dst, m, x0, x1, z0, z1,
                   new_lon_min, new_lon_max, new_lat_min, new_lat_max,
                   new_width, new_height, ortho_w, ortho_h):
    """Produit ortho.jpg recadrée. Par défaut, on ne recadre PAS l'ortho basse
       résolution de la carte source : celle-ci partage son budget de pixels
       WMS (limite serveur IGN ~5010 px) avec toute l'emprise de la grande
       carte (des dizaines de km), ce qui donne un sol flou vu de près une
       fois recadré sur une arène de ~2 km. On réémet donc une requête WMS
       dédiée, centrée sur la seule boîte de l'arène : le même budget de
       pixels serveur, appliqué à une emprise bien plus petite, donne une
       résolution nettement meilleure (voir --ortho-px). --offline-crop
       revient au comportement historique (crop hors ligne, sans requête).
       Renvoie (new_ortho_w, new_ortho_h)."""
    if args.offline_crop:
        ox_lo = round((x0 + grid.half_w) / grid.width_m * ortho_w)
        ox_hi = round((x1 + grid.half_w) / grid.width_m * ortho_w)
        oy_lo = round((z0 + grid.half_h) / grid.height_m * ortho_h)
        oy_hi = round((z1 + grid.half_h) / grid.height_m * ortho_h)
        ortho = Image.open(src / "ortho.jpg").convert("RGB")
        ortho.crop((ox_lo, oy_lo, ox_hi, oy_hi)).save(dst / "ortho.jpg", quality=92)
        return ox_hi - ox_lo, oy_hi - oy_lo

    config.LON_MIN, config.LON_MAX = new_lon_min, new_lon_max
    config.LAT_MIN, config.LAT_MAX = new_lat_min, new_lat_max
    config.RECOLOR_SEA = m.get("sea", "0") == "1"
    config.ORTHO_HEIGHT = args.ortho_px
    config.OUT_DIR = str(dst)
    return fetch_ortho(new_width / new_height), args.ortho_px


def write_cropped_meta(dst, src, m, new_cols, new_rows, elev_min, elev_max,
                        new_lon_min, new_lon_max, new_lat_min, new_lat_max,
                        new_width, new_height, new_ortho_w, new_ortho_h,
                        origin_x, origin_z):
    """Écrit le terrain.txt recadré (même repère monde que la carte source,
       via origin_x/origin_z : voir le docstring du module)."""
    lines = [
        f"# Terrain Artouste - recadré depuis {src.name} (mode zombie, sous-région)",
        "# Repère monde identique à la carte source (origin_x/origin_z) : tous les",
        "# fichiers en coordonnées monde restent valides sans décalage.",
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
        f"sea {m.get('sea', '0')}",
        f"start_x {m.get('start_x', '0')}",
        f"start_z {m.get('start_z', '0')}",
        f"start_heading {m.get('start_heading', '0')}",
        f"origin_x {origin_x:.2f}",
        f"origin_z {origin_z:.2f}",
    ]
    # encoding explicite : la première ligne sert de titre au menu et porte des
    # accents, on ne dépend pas de l'encodage par défaut de la plateforme.
    (dst / "terrain.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def copy_aux_files(src, dst):
    """Copie tels quels les fichiers annexes en coordonnées monde ou lon/lat
       (voir FICHIERS_ANNEXES), s'ils existent sur la carte source."""
    for aux in FICHIERS_ANNEXES:
        p = src / aux
        if p.exists():
            shutil.copy2(p, dst / aux)


def main():
    args = parse_args()
    src, dst = args.src, args.dst
    m = read_meta(src / "terrain.txt")
    grid = Grid(int(m["cols"]), int(m["rows"]), float(m["width_m"]), float(m["height_m"]),
                float(m["lon_min"]), float(m["lon_max"]), float(m["lat_min"]), float(m["lat_max"]))
    elev_min, elev_max = float(m["elev_min"]), float(m["elev_max"])
    ortho_w, ortho_h = int(m["ortho_width"]), int(m["ortho_height"])

    i_lo, i_hi, j_lo, j_hi = crop_bounds(grid, args)
    new_cols, new_rows = i_hi - i_lo + 1, j_hi - j_lo + 1

    # Étendue monde réelle de la grille recadrée (bornes des colonnes/rangées).
    x0, x1 = grid.x_at(i_lo), grid.x_at(i_hi)
    z0, z1 = grid.z_at(j_lo), grid.z_at(j_hi)
    new_width = x1 - x0
    new_height = z1 - z0
    origin_x = 0.5 * (x0 + x1)  # centre de la boîte en coord MONDE
    origin_z = 0.5 * (z0 + z1)

    # Nouvelles bornes lon/lat (mêmes formules que render::Terrain).
    new_lon_min, new_lon_max = grid.lon_of(x0), grid.lon_of(x1)
    new_lat_max, new_lat_min = grid.lat_of(z0), grid.lat_of(z1)  # z0=nord -> lat max

    dst.mkdir(parents=True, exist_ok=True)

    crop_heightmap(src, dst, i_lo, i_hi, j_lo, j_hi)
    new_ortho_w, new_ortho_h = produce_ortho(
        args, grid, src, dst, m, x0, x1, z0, z1,
        new_lon_min, new_lon_max, new_lat_min, new_lat_max,
        new_width, new_height, ortho_w, ortho_h)

    write_cropped_meta(dst, src, m, new_cols, new_rows, elev_min, elev_max,
                        new_lon_min, new_lon_max, new_lat_min, new_lat_max,
                        new_width, new_height, new_ortho_w, new_ortho_h,
                        origin_x, origin_z)
    copy_aux_files(src, dst)

    print(f"[crop] {src.name} -> {dst.name}")
    print(f"  grille {new_cols}x{new_rows} ({new_width:.0f}x{new_height:.0f} m), "
          f"ortho {new_ortho_w}x{new_ortho_h}")
    print(f"  origin monde ({origin_x:.1f}, {origin_z:.1f})")


if __name__ == "__main__":
    main()
