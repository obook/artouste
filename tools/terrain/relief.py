# -*- coding: utf-8 -*-
"""
relief.py
Téléchargement du relief depuis l'API altimétrie de l'IGN (RGE ALTI),
enregistrement en carte d'altitude PNG 16 bits, et calage du point de départ
sur le replat le plus proche.

Auteur : O. Booklage
Licence : GPL v2
"""

import json
import os
import time
import urllib.error
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor

import numpy as np
from PIL import Image

from terrain import config


def fetch_chunk(lat, lons):
    """Récupère les altitudes d'un morceau de rangée (une seule requête)."""
    query = urllib.parse.urlencode({
        "lon": "|".join(f"{v:.6f}" for v in lons),
        "lat": "|".join(f"{lat:.6f}" for _ in lons),
        "resource": config.ALTI_RESOURCE,
        "delimiter": "|",
        "indent": "false",
        "zonly": "true",
    })
    url = config.ALTI_URL + "?" + query
    last_err = None
    for attempt in range(8):
        try:
            with urllib.request.urlopen(url, timeout=60) as resp:
                data = json.loads(resp.read())
            return [float(z) for z in data["elevations"]]
        except urllib.error.HTTPError as err:
            last_err = err
            # 429 = trop de requêtes : on attend plus longtemps avant de retenter.
            time.sleep((5.0 if err.code == 429 else 2.0) * (attempt + 1))
        except Exception as err:  # réseau capricieux : on retente
            last_err = err
            time.sleep(1.0 + attempt)
    raise RuntimeError(f"morceau de rangée : échec après plusieurs essais ({last_err})")


def fetch_row(j):
    """Récupère les COLS altitudes d'une rangée (latitude constante), en autant de
       requêtes que nécessaire pour ne pas dépasser la limite de longueur d'URL."""
    lat = config.LAT_MAX - (config.LAT_MAX - config.LAT_MIN) * j / (config.ROWS - 1)
    lons = [config.LON_MIN + (config.LON_MAX - config.LON_MIN) * i / (config.COLS - 1)
            for i in range(config.COLS)]
    elevations = []
    for k in range(0, config.COLS, config.MAX_PTS_PER_REQUEST):
        elevations.extend(fetch_chunk(lat, lons[k:k + config.MAX_PTS_PER_REQUEST]))
    return j, elevations


def fetch_heightmap():
    """Télécharge toute la grille d'altitude en parallèle, par rangées."""
    print(f"[relief] grille {config.COLS}x{config.ROWS} sur {config.ALTI_RESOURCE}...")
    grid = [None] * config.ROWS
    done = 0
    with ThreadPoolExecutor(max_workers=2) as pool:
        for j, row in pool.map(fetch_row, range(config.ROWS)):
            grid[j] = row
            done += 1
            if done % 16 == 0 or done == config.ROWS:
                print(f"[relief]   {done}/{config.ROWS} rangées")
    return grid


def write_heightmap(grid):
    """Normalise la grille et l'écrit en PNG 16 bits ; renvoie (elev_min, elev_max)."""
    # La mer (NODATA) et les rares valeurs négatives sont ramenées à 0 m.
    cleaned = np.array([[max(0.0, z) if z > config.NODATA else 0.0 for z in row] for row in grid],
                       dtype=np.float32)
    elev_max = float(cleaned.max())
    elev_min = 0.0
    span = elev_max - elev_min if elev_max > elev_min else 1.0

    levels = np.round((cleaned - elev_min) / span * 65535.0).astype(np.uint16)
    path = os.path.join(config.OUT_DIR, "heightmap.png")
    Image.fromarray(levels, mode="I;16").save(path)
    print(f"[relief] {path} écrit (altitude 0 -> {elev_max:.1f} m)")
    return elev_min, elev_max


def find_flat_start(grid, width_m, height_m):
    """Cale le point de départ sur le replat le plus proche de START_LON/LAT : la
       cellule dont le voisinage (bloc 3x3, ~100 m) a le plus faible dénivelé. Ainsi
       l'hélipad se pose bien à plat (et non en travers ni flottant sur un flanc)."""
    arr = np.array([[max(0.0, z) if z > config.NODATA else 0.0 for z in row] for row in grid],
                   dtype=np.float32)
    col0 = int(round((config.START_LON - config.LON_MIN)
                     / (config.LON_MAX - config.LON_MIN) * (config.COLS - 1)))
    row0 = int(round((config.LAT_MAX - config.START_LAT)
                     / (config.LAT_MAX - config.LAT_MIN) * (config.ROWS - 1)))
    win = 18  # cellules autour de la cible (~650 m)
    best = None
    for r in range(max(1, row0 - win), min(config.ROWS - 1, row0 + win + 1)):
        for c in range(max(1, col0 - win), min(config.COLS - 1, col0 + win + 1)):
            block = arr[r - 1:r + 2, c - 1:c + 2]
            rough = float(block.max() - block.min())  # dénivelé du voisinage
            if best is None or rough < best[0]:
                best = (rough, r, c)
    _, r, c = best
    start_x = (c / (config.COLS - 1) - 0.5) * width_m   # colonne 0 = ouest, dernière = est
    start_z = (r / (config.ROWS - 1) - 0.5) * height_m   # rangée 0 = nord, dernière = sud
    print(f"[depart] replat à la cellule ({c},{r}), dénivelé voisinage {best[0]:.1f} m, "
          f"altitude {arr[r, c]:.0f} m")
    return start_x, start_z
