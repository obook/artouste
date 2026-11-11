#!/usr/bin/env python3
"""
fetch_lampadaires.py
Fabrique les points d'éclairage public d'une carte (lampadaires.bin) : vu
d'hélicoptère, une ville la nuit est un chapelet de points orangés qui dessine
les rues. On pose un point tous les PAS_M mètres le long des tronçons de route
URBAINS de la BD TOPO (champ "urbain", Géoplateforme IGN, Licence Ouverte
Etalab 2.0). Le moteur les dessine en points lumineux la nuit (render/Lampadaires).

Format de lampadaires.bin (petit-boutiste) :
    magie   : 4 octets "ALMP"
    version : uint32 (= 1)
    nombre  : uint32
    puis, par point : lon float32, lat float32 (degrés WGS84)

Usage : python3 tools/fetch_lampadaires.py [carte]   (carte par défaut : ossau)
Sortie : assets/terrain/<carte>/lampadaires.bin

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import os
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from terrain.config import TERRAIN_ROOT
from terrain.meta import read_meta

from foret.geometrie import lignes_de
from foret.reglages import NATURES_ROUTE, WFS_LAYER_ROUTE
from foret.service_ign import fetch_pages, filtre_natures

PAS_M = 30.0            # un lampadaire tous les 30 m
M_PAR_DEG_LAT = 111320.0


def echantillonner(ligne, pas, m_par_deg_lon):
    """Points tous les pas mètres le long d'une ligne (lon, lat), à partir d'un
       demi-pas : deux tronçons bout à bout ne doublent pas le lampadaire de
       leur jonction."""
    points = []
    prochain = pas / 2.0   # abscisse curviligne du prochain point à poser
    parcouru = 0.0
    for (lon0, lat0, *_), (lon1, lat1, *_) in zip(ligne, ligne[1:]):
        dx = (lon1 - lon0) * m_par_deg_lon
        dy = (lat1 - lat0) * M_PAR_DEG_LAT
        longueur = math.hypot(dx, dy)
        while longueur > 0 and prochain <= parcouru + longueur:
            t = (prochain - parcouru) / longueur
            points.append((lon0 + (lon1 - lon0) * t, lat0 + (lat1 - lat0) * t))
            prochain += pas
        parcouru += longueur
    return points


def main():
    carte = sys.argv[1] if len(sys.argv) > 1 else "ossau"
    out_dir = Path(TERRAIN_ROOT) / carte
    meta_path = out_dir / "terrain.txt"
    if not meta_path.exists():
        raise RuntimeError(f"carte inconnue : {carte} (pas de {meta_path})")
    meta = read_meta(meta_path)
    lon_min, lon_max = float(meta["lon_min"]), float(meta["lon_max"])
    lat_min, lat_max = float(meta["lat_min"]), float(meta["lat_max"])
    bbox = f"{lon_min},{lat_min},{lon_max},{lat_max}"
    m_par_deg_lon = M_PAR_DEG_LAT * math.cos(math.radians(0.5 * (lat_min + lat_max)))
    depart = time.time()

    points = []
    troncons = 0
    for feat in fetch_pages(bbox, WFS_LAYER_ROUTE, "nature,urbain,geometrie",
                            filtre=filtre_natures(bbox, NATURES_ROUTE)):
        if not feat["properties"].get("urbain"):
            continue
        troncons += 1
        for ligne in lignes_de(feat.get("geometry")):
            points += [p for p in echantillonner(ligne, PAS_M, m_par_deg_lon)
                       if lon_min <= p[0] <= lon_max and lat_min <= p[1] <= lat_max]

    path = out_dir / "lampadaires.bin"
    with open(path, "wb") as f:
        f.write(b"ALMP" + struct.pack("<II", 1, len(points)))
        for lon, lat in points:
            f.write(struct.pack("<ff", lon, lat))
    print(f"[lampadaires] {path} écrit : {len(points)} points sur {troncons} tronçons "
          f"urbains, {os.path.getsize(path) / 1024.0:.0f} ko, {time.time() - depart:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
