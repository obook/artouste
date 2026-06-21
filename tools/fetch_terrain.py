#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fetch_terrain.py
Télécharge les données du terrain réel d'Artouste depuis la Géoplateforme IGN,
sur la vallée d'Ossau (lac d'Artouste, pic du Midi d'Ossau, ~2884 m) :

  - le relief, échantillonné sur une grille via l'API altimétrie (RGE ALTI),
    enregistré en carte d'altitude PNG 16 bits (heightmap.png) ;
  - l'orthophoto (BD ORTHO) via le service WMS, enregistrée en ortho.jpg ;
  - les métadonnées de calage (terrain.txt) lues par le moteur.

Zone de montagne sans mer (RECOLOR_SEA = False) : on ne bleuit donc pas le blanc
de la photo, qui serait de la neige ou des glaciers. Le blanc de no-data en
bordure de couverture est comblé par une teinte de rocaille (voir fill_nodata).

Données : IGN Géoplateforme, Licence Ouverte Etalab 2.0.
Dépendances : Python 3, Pillow (PIL), NumPy, SciPy. Aucun GDAL requis.

Usage : python3 tools/fetch_terrain.py
Sortie : assets/terrain/{heightmap.png, ortho.jpg, terrain.txt}

Auteur : O. Booklage
Licence : GPL v2
"""

import json
import math
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor

import io

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

# --- Zone et résolution ------------------------------------------------------
# Emprise géographique (WGS84) : vallée d'Ossau, autour du lac d'Artouste et du
# pic du Midi d'Ossau (2884 m). C'est le lieu-titre du projet (la turbine Artouste
# de l'Alouette II est nommée d'après ce massif). Entièrement en France.
LON_MIN, LON_MAX = -0.52, -0.30
LAT_MIN, LAT_MAX = 42.80, 42.96

# Nombre de points de la grille d'altitude (et de sommets du maillage).
COLS = 512  # axe ouest -> est (longitude)
ROWS = 512  # axe nord -> sud (latitude)

# Taille de l'orthophoto téléchargée (le moteur la drape sur le maillage).
ORTHO_HEIGHT = 2048

# Recoloration de la mer : utile en bord de mer (la BD ORTHO rend la mer en
# blanc), mais à éviter en montagne, où le blanc est de la neige ou des glaciers
# qu'il ne faut surtout pas bleuir. À mettre à False pour une zone sans mer.
# RECOLOR_SEA pilote aussi le plan de mer du moteur (clé "sea" du calage) : pas de
# mer en montagne.
RECOLOR_SEA = False

# Point de départ du vol (WGS84) : Fabrèges, le fond de vallée plat à l'entrée du
# massif (lac de Fabrèges, station du téléphérique d'Artouste, ~1230 m). Le moteur
# lit cette position (clés start_x / start_z du calage) et y pose l'appareil. Le
# script affine ensuite sur le replat le plus proche (voir find_flat_start).
START_LON, START_LAT = -0.413, 42.905

# Couleur de repli de la mer si l'on ne trouve pas assez de pixels d'eau
# photographiés pour la mesurer. La vraie couleur est échantillonnée sur la mer
# de la photo (voir fetch_ortho) ; elle doit rester proche du plan de mer du
# moteur (SEA_COLOR dans Application.cpp).
SEA_FALLBACK = (43, 65, 70)

# --- Services IGN ------------------------------------------------------------
ALTI_URL = "https://data.geopf.fr/altimetrie/1.0/calcul/alti/rest/elevation.json"
ALTI_RESOURCE = "ign_rge_alti_wld"
WMS_URL = "https://data.geopf.fr/wms-r/wms"
WMS_LAYER = "ORTHOIMAGERY.ORTHOPHOTOS"

# Valeur renvoyée par l'API là où il n'y a pas de donnée terrestre (mer) ; on la
# ramène au niveau de la mer (0 m).
NODATA = -1000.0

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "terrain")


# Nombre maximal de points par requête : au-delà, l'URL de l'API dépasse sa limite
# de longueur (erreur HTTP 414). On découpe donc une rangée en plusieurs morceaux.
MAX_PTS_PER_REQUEST = 200


def fetch_chunk(lat, lons):
    """Récupère les altitudes d'un morceau de rangée (une seule requête)."""
    query = urllib.parse.urlencode({
        "lon": "|".join(f"{v:.6f}" for v in lons),
        "lat": "|".join(f"{lat:.6f}" for _ in lons),
        "resource": ALTI_RESOURCE,
        "delimiter": "|",
        "indent": "false",
        "zonly": "true",
    })
    url = ALTI_URL + "?" + query
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
    lat = LAT_MAX - (LAT_MAX - LAT_MIN) * j / (ROWS - 1)
    lons = [LON_MIN + (LON_MAX - LON_MIN) * i / (COLS - 1) for i in range(COLS)]
    elevations = []
    for k in range(0, COLS, MAX_PTS_PER_REQUEST):
        elevations.extend(fetch_chunk(lat, lons[k:k + MAX_PTS_PER_REQUEST]))
    return j, elevations


def fetch_heightmap():
    """Télécharge toute la grille d'altitude en parallèle, par rangées."""
    print(f"[relief] grille {COLS}x{ROWS} sur {ALTI_RESOURCE}...")
    grid = [None] * ROWS
    done = 0
    with ThreadPoolExecutor(max_workers=2) as pool:
        for j, row in pool.map(fetch_row, range(ROWS)):
            grid[j] = row
            done += 1
            if done % 16 == 0 or done == ROWS:
                print(f"[relief]   {done}/{ROWS} rangées")
    return grid


def write_heightmap(grid):
    """Normalise la grille et l'écrit en PNG 16 bits ; renvoie (elev_min, elev_max)."""
    # La mer (NODATA) et les rares valeurs négatives sont ramenées à 0 m.
    cleaned = np.array([[max(0.0, z) if z > NODATA else 0.0 for z in row] for row in grid],
                       dtype=np.float32)
    elev_max = float(cleaned.max())
    elev_min = 0.0
    span = elev_max - elev_min if elev_max > elev_min else 1.0

    levels = np.round((cleaned - elev_min) / span * 65535.0).astype(np.uint16)
    path = os.path.join(OUT_DIR, "heightmap.png")
    Image.fromarray(levels, mode="I;16").save(path)
    print(f"[relief] {path} écrit (altitude 0 -> {elev_max:.1f} m)")
    return elev_min, elev_max


def fill_nodata(arr):
    """Comble le no-data de la BD ORTHO (blanc pur en bordure de couverture) par une
       teinte de rocaille. On ne remplit que les grandes plages blanches connectées
       au bord de l'image : la neige des sommets (blanche mais à l'intérieur) est
       épargnée."""
    white = (arr[:, :, 0] >= 248) & (arr[:, :, 1] >= 248) & (arr[:, :, 2] >= 248)
    labels, count = ndimage.label(white)
    if count == 0:
        return arr
    border = set(labels[0, :]) | set(labels[-1, :]) | set(labels[:, 0]) | set(labels[:, -1])
    border.discard(0)
    sizes = ndimage.sum(np.ones_like(labels), labels, index=range(1, count + 1))
    keep = [lab for lab in border if sizes[lab - 1] > 3000]
    nodata = np.isin(labels, keep)
    if not nodata.any():
        return arr
    rock = np.median(arr[~white], axis=0) * 0.9  # teinte du terrain, assombrie
    out = arr.copy()
    out[nodata] = rock.astype(np.uint8)
    print(f"[ortho] no-data comblé ({int(nodata.sum())} px) couleur {rock.round().astype(int).tolist()}")
    return out


def fetch_ortho(aspect):
    """Télécharge l'orthophoto sur la même emprise (haut = nord) et recolore la mer."""
    width = int(round(ORTHO_HEIGHT * aspect))
    print(f"[ortho] WMS {width}x{ORTHO_HEIGHT} sur {WMS_LAYER}...")
    query = urllib.parse.urlencode({
        "SERVICE": "WMS",
        "VERSION": "1.3.0",
        "REQUEST": "GetMap",
        "LAYERS": WMS_LAYER,
        "STYLES": "",
        "CRS": "EPSG:4326",            # axes lat,lon en WMS 1.3.0
        "BBOX": f"{LAT_MIN},{LON_MIN},{LAT_MAX},{LON_MAX}",
        "WIDTH": width,
        "HEIGHT": ORTHO_HEIGHT,
        "FORMAT": "image/jpeg",
    })
    url = WMS_URL + "?" + query
    with urllib.request.urlopen(url, timeout=120) as resp:
        content = resp.read()
    if content[:2] != b"\xff\xd8":  # pas un JPEG : c'est une erreur du service
        raise RuntimeError("le WMS n'a pas renvoyé une image : "
                           + content[:200].decode("utf-8", "replace"))
    path = os.path.join(OUT_DIR, "ortho.jpg")

    # Zone sans mer (montagne) : pas de recoloration de la mer (qui bleuirait la
    # neige) ; on comble seulement le no-data de la BD ORTHO par de la rocaille.
    if not RECOLOR_SEA:
        arr = np.array(Image.open(io.BytesIO(content)).convert("RGB"))
        Image.fromarray(fill_nodata(arr)).save(path, quality=88)
        print(f"[ortho] {path} écrit ({width}x{ORTHO_HEIGHT})")
        return width

    raw = os.path.join(OUT_DIR, "_ortho_raw.jpg")
    with open(raw, "wb") as out:
        out.write(content)

    # La mer de la BD ORTHO pose deux problèmes : au large elle revient en blanc
    # (sans donnée), et l'eau photographiée est une mosaïque de dalles aux teintes
    # différentes, dont les bords en escalier formeraient des "pavés" au rendu.
    # On aplanit donc toute la mer à une couleur unie, en préservant l'écume
    # blanche et la plage (non bleutées) pour garder un trait de côte net.
    arr = np.array(Image.open(raw).convert("RGB")).astype(np.float32)
    r, g, b = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
    nodata = (r > 250) & (g > 250) & (b > 250)
    water = (b > r + 2) & (b > g - 4) & (arr.max(axis=2) < 155)  # eau bleutée, pas l'écume
    sea = nodata | water

    photographed = water & ~nodata
    deep = np.median(arr[photographed], axis=0) if int(photographed.sum()) > 1000 \
        else np.array(SEA_FALLBACK, dtype=np.float32)

    out = arr.copy()
    out[nodata] = deep
    alpha = np.asarray(Image.fromarray((sea * 255).astype(np.uint8))
                       .filter(ImageFilter.GaussianBlur(4)), dtype=np.float32) / 255.0
    alpha = alpha[:, :, None]
    out = out * (1.0 - alpha) + deep * alpha
    print(f"[ortho] mer aplanie ({int(sea.sum())} px) couleur {deep.round().astype(int).tolist()}")

    Image.fromarray(out.clip(0, 255).astype(np.uint8)).save(path, quality=88)
    os.remove(raw)
    print(f"[ortho] {path} écrit ({width}x{ORTHO_HEIGHT})")
    return width


def find_flat_start(grid, width_m, height_m):
    """Cale le point de départ sur le replat le plus proche de START_LON/LAT : la
       cellule dont le voisinage (bloc 3x3, ~100 m) a le plus faible dénivelé. Ainsi
       l'hélipad se pose bien à plat (et non en travers ni flottant sur un flanc)."""
    arr = np.array([[max(0.0, z) if z > NODATA else 0.0 for z in row] for row in grid],
                   dtype=np.float32)
    col0 = int(round((START_LON - LON_MIN) / (LON_MAX - LON_MIN) * (COLS - 1)))
    row0 = int(round((LAT_MAX - START_LAT) / (LAT_MAX - LAT_MIN) * (ROWS - 1)))
    win = 18  # cellules autour de la cible (~650 m)
    best = None
    for r in range(max(1, row0 - win), min(ROWS - 1, row0 + win + 1)):
        for c in range(max(1, col0 - win), min(COLS - 1, col0 + win + 1)):
            block = arr[r - 1:r + 2, c - 1:c + 2]
            rough = float(block.max() - block.min())  # dénivelé du voisinage
            if best is None or rough < best[0]:
                best = (rough, r, c)
    _, r, c = best
    start_x = (c / (COLS - 1) - 0.5) * width_m   # colonne 0 = ouest, dernière = est
    start_z = (r / (ROWS - 1) - 0.5) * height_m   # rangée 0 = nord, dernière = sud
    print(f"[depart] replat à la cellule ({c},{r}), dénivelé voisinage {best[0]:.1f} m, "
          f"altitude {arr[r, c]:.0f} m")
    return start_x, start_z


def write_metadata(elev_min, elev_max, width_m, height_m, ortho_w, start_x, start_z):
    """Écrit le fichier de calage lu par le moteur (clés simples, une par ligne)."""
    path = os.path.join(OUT_DIR, "terrain.txt")
    with open(path, "w", encoding="utf-8") as out:
        out.write("# Terrain Artouste - vallée d'Ossau (lac d'Artouste, pic du Midi d'Ossau)\n")
        out.write("# Données IGN Géoplateforme (RGE ALTI + BD ORTHO), Licence Ouverte Etalab 2.0\n")
        out.write("# width_m / height_m : dimensions au sol du maillage, en mètres\n")
        out.write(f"cols {COLS}\n")
        out.write(f"rows {ROWS}\n")
        out.write(f"width_m {width_m:.1f}\n")
        out.write(f"height_m {height_m:.1f}\n")
        out.write(f"elev_min {elev_min:.2f}\n")
        out.write(f"elev_max {elev_max:.2f}\n")
        out.write(f"lon_min {LON_MIN}\n")
        out.write(f"lon_max {LON_MAX}\n")
        out.write(f"lat_min {LAT_MIN}\n")
        out.write(f"lat_max {LAT_MAX}\n")
        out.write(f"ortho_width {ortho_w}\n")
        out.write(f"ortho_height {ORTHO_HEIGHT}\n")
        # Plan de mer du moteur : oui en bord de mer, non en montagne.
        out.write(f"sea {1 if RECOLOR_SEA else 0}\n")
        # Point de départ (replat) en coordonnées monde (X est, Z sud).
        out.write(f"start_x {start_x:.1f}\n")
        out.write(f"start_z {start_z:.1f}\n")
    print(f"[meta] {path} écrit")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    # Dimensions métriques de l'emprise (projection équirectangulaire locale,
    # très précise sur ~20 km). X = est, Z = sud dans le moteur.
    lat_center = math.radians((LAT_MIN + LAT_MAX) / 2.0)
    m_per_deg_lat = 111320.0
    m_per_deg_lon = 111320.0 * math.cos(lat_center)
    width_m = (LON_MAX - LON_MIN) * m_per_deg_lon
    height_m = (LAT_MAX - LAT_MIN) * m_per_deg_lat
    print(f"[zone] emprise ~ {width_m / 1000:.1f} km (E-O) x {height_m / 1000:.1f} km (N-S)")

    start = time.time()
    grid = fetch_heightmap()
    elev_min, elev_max = write_heightmap(grid)
    ortho_w = fetch_ortho(width_m / height_m)
    start_x, start_z = find_flat_start(grid, width_m, height_m)
    write_metadata(elev_min, elev_max, width_m, height_m, ortho_w, start_x, start_z)
    print(f"[ok] terminé en {time.time() - start:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
