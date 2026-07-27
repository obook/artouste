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


def fetch_heightmap_api():
    """Télécharge toute la grille d'altitude en parallèle, par rangées, via l'API
       altimétrie point par point. Repli : voir fetch_heightmap()."""
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


def _noeud_lon(i):
    """Longitude du point de grille i (colonne 0 = ouest, dernière = est)."""
    return config.LON_MIN + (config.LON_MAX - config.LON_MIN) * i / (config.COLS - 1)


def _noeud_lat(j):
    """Latitude du point de grille j (rangée 0 = nord, dernière = sud)."""
    return config.LAT_MAX - (config.LAT_MAX - config.LAT_MIN) * j / (config.ROWS - 1)


def _bloc_raster(c0, c1, r0, r1, surech=1):
    """Une requête de relief en BIL sur un rectangle de la grille, renvoyée en
       tableau numpy de flottants (rangée 0 au nord), à raison d'une valeur par
       point de grille.

       Calage : nos points de grille sont des NOEUDS, le premier sur la bordure
       ouest et le dernier sur la bordure est. Le service, lui, rend des PIXELS,
       dont les centres tombent à un demi-pas à l'intérieur de l'emprise
       demandée. On élargit donc l'emprise d'un demi-pas de chaque côté : la
       maille rendue coïncide alors exactement avec la nôtre.

       Suréchantillonnage : on demande surech fois plus de pixels que de points,
       puis on moyenne chaque paquet. Voir ALTI_SURECHANTILLONNAGE (config.py)
       pour la raison et les mesures."""
    pas_lon = (config.LON_MAX - config.LON_MIN) / (config.COLS - 1)
    pas_lat = (config.LAT_MAX - config.LAT_MIN) / (config.ROWS - 1)
    lon_lo = _noeud_lon(c0) - pas_lon / 2.0
    lon_hi = _noeud_lon(c1 - 1) + pas_lon / 2.0
    lat_hi = _noeud_lat(r0) + pas_lat / 2.0
    lat_lo = _noeud_lat(r1 - 1) - pas_lat / 2.0
    points_x, points_y = c1 - c0, r1 - r0
    largeur, hauteur = points_x * surech, points_y * surech

    query = urllib.parse.urlencode({
        "SERVICE": "WMS",
        "VERSION": "1.3.0",
        "REQUEST": "GetMap",
        "LAYERS": config.ALTI_WMS_LAYER,
        "STYLES": "",
        "CRS": "EPSG:4326",            # axes lat,lon en WMS 1.3.0
        "BBOX": f"{lat_lo},{lon_lo},{lat_hi},{lon_hi}",
        "WIDTH": largeur,
        "HEIGHT": hauteur,
        "FORMAT": config.ALTI_WMS_FORMAT,
    })
    url = config.WMS_URL + "?" + query
    attendu = largeur * hauteur * 4

    last_err = None
    for attempt in range(4):
        try:
            with urllib.request.urlopen(url, timeout=300) as resp:
                data = resp.read()
            if len(data) != attendu:
                # Le service répond en XML quand il refuse : on le remonte tel quel.
                raise RuntimeError(f"{len(data)} octets reçus au lieu de {attendu} "
                                   f"({data[:160].decode('utf-8', 'replace')})")
            brut = np.frombuffer(data, dtype="<f4").reshape(hauteur, largeur)
            if surech == 1:
                return brut
            # Les points sans donnée valent une valeur sentinelle très négative :
            # les moyenner avec leurs voisins valides donnerait n'importe quoi.
            # On les écarte de la moyenne, et une maille entièrement sans donnée
            # ressort à la sentinelle, que le nettoyage ramènera au niveau 0.
            valide = np.where(brut > config.NODATA, brut, np.nan)
            paquets = valide.reshape(points_y, surech, points_x, surech)
            with np.errstate(invalid="ignore"):
                moyenne = np.nanmean(paquets, axis=(1, 3))
            return np.where(np.isnan(moyenne), config.NODATA - 1.0, moyenne).astype(np.float32)
        except Exception as err:
            last_err = err
            time.sleep(2.0 * (attempt + 1))
    raise RuntimeError(f"bloc de relief {largeur}x{hauteur} : échec ({last_err})")


def fetch_heightmap_raster():
    """Télécharge toute la grille d'altitude en quelques requêtes raster."""
    surech = max(1, config.ALTI_SURECHANTILLONNAGE)
    # Le plafond porte sur les pixels demandés, pas sur les points de grille :
    # c'est la taille de la réponse qu'il s'agit de borner.
    pas = max(1, config.ALTI_MAX_PX // surech)
    blocs_x = list(range(0, config.COLS, pas))
    blocs_y = list(range(0, config.ROWS, pas))
    total = len(blocs_x) * len(blocs_y)
    octets = config.COLS * config.ROWS * surech * surech * 4
    print(f"[relief] grille {config.COLS}x{config.ROWS} en {total} requête(s) raster "
          f"sur {config.ALTI_WMS_LAYER}")
    print(f"[relief] suréchantillonnage x{surech} : {octets / 1e6:.0f} Mo à recevoir")

    grille = np.zeros((config.ROWS, config.COLS), dtype=np.float32)
    fait = 0
    for r0 in blocs_y:
        r1 = min(r0 + pas, config.ROWS)
        for c0 in blocs_x:
            c1 = min(c0 + pas, config.COLS)
            grille[r0:r1, c0:c1] = _bloc_raster(c0, c1, r0, r1, surech)
            fait += 1
            print(f"[relief]   {fait}/{total} ({c1 - c0}x{r1 - r0} points)")
    return grille.tolist()


def fetch_heightmap():
    """Télécharge toute la grille d'altitude.

       Par raster d'abord : quelques requêtes au lieu de plusieurs milliers, des
       secondes au lieu de longues minutes. Si la couche raster est indisponible
       (service modifié, réseau capricieux), on retombe sur l'API altimétrie
       point par point, qui rend le même résultat en beaucoup plus de temps."""
    try:
        return fetch_heightmap_raster()
    except Exception as err:
        print(f"[relief] raster indisponible ({err})")
        print("[relief] repli sur l'API altimétrie, point par point (c'est long)")
        return fetch_heightmap_api()


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
