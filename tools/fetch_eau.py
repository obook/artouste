#!/usr/bin/env python3
"""
fetch_eau.py
Fabrique le masque d'eau d'une carte (eau.png), que terrain.frag consulte pour
poser le reflet du soleil : plans d'eau, lacs, rivières et bassins de la BD TOPO
(surface_hydrographique, Géoplateforme IGN, Licence Ouverte Etalab 2.0), plus
la mer hors du territoire des départements (ADMIN EXPRESS) sur les cartes qui
dessinent un plan de mer (clé "sea" de terrain.txt), moins l'estran (zone
découverte à marée basse : la BD TOPO compte tout le bassin d'Arcachon comme
eau, alors que l'orthophoto montre ses bancs de sable). Sans ce masque, le moteur
reconnaissait l'eau à sa couleur dans l'orthophoto, et le bassin d'Arcachon,
gris-vert sur la photo, n'en était pas.

Même emprise et même grille que forest.png (un pixel tous les PX_M mètres) :
le moteur l'échantillonne avec les coordonnées de l'orthophoto, sans recalage.

Format de eau.png : niveaux de gris, 255 sur l'eau, 0 ailleurs.

Usage : python3 tools/fetch_eau.py [carte]   (carte par défaut : ossau)
Sortie : assets/terrain/<carte>/eau.png

Auteur : O. Booklage
Licence : GPL v2
"""

import os
import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from terrain.config import TERRAIN_ROOT
from terrain.meta import read_meta

from foret.masque import peindre
from foret.reglages import PX_M, WFS_LAYER_DEPT
from foret.service_ign import fetch_pages

WFS_LAYER_EAU = "BDTOPO_V3:surface_hydrographique"
WFS_LAYER_ESTRAN = "BDTOPO_V3:zone_d_estran"
EAU = 255


def main():
    carte = sys.argv[1] if len(sys.argv) > 1 else "ossau"
    out_dir = Path(TERRAIN_ROOT) / carte
    meta_path = out_dir / "terrain.txt"
    if not meta_path.exists():
        raise RuntimeError(f"carte inconnue : {carte} (pas de {meta_path})")
    meta = read_meta(meta_path)

    lon_min, lon_max = float(meta["lon_min"]), float(meta["lon_max"])
    lat_min, lat_max = float(meta["lat_min"]), float(meta["lat_max"])
    width_m, height_m = float(meta["width_m"]), float(meta["height_m"])
    bbox = f"{lon_min},{lat_min},{lon_max},{lat_max}"
    cols = max(1, round(width_m / PX_M))
    rows = max(1, round(height_m / PX_M))
    cadre = (lon_min, lon_max, lat_min, lat_max, cols, rows)
    print(f"[eau] carte {carte} : emprise {bbox}, masque {cols}x{rows} ({PX_M:.0f} m/px)")
    depart = time.time()

    # Les surfaces intermittentes (lit asséché l'été) ne reflètent rien.
    eau, n_surfaces = peindre(cadre, fetch_pages(bbox, WFS_LAYER_EAU, "persistance,geometrie"),
                              lambda p: 0 if p.get("persistance") == "Intermittent" else EAU)
    print(f"[eau]   {n_surfaces} surfaces hydrographiques (BD TOPO)")

    if meta.get("sea", "0") == "1":
        # Mer : tout ce qui n'est dans aucun département. Vaut sur une côte,
        # pas en montagne où l'étranger est de la terre (d'où la clé "sea").
        france, n_dept = peindre(cadre, fetch_pages(bbox, WFS_LAYER_DEPT), lambda p: 1)
        eau = np.where(france > 0, eau, EAU)
        print(f"[eau]   mer hors des {n_dept} contours de départements")

    estran, n_estran = peindre(cadre, fetch_pages(bbox, WFS_LAYER_ESTRAN), lambda p: 1)
    if n_estran:
        eau = np.where(estran > 0, 0, eau)
        print(f"[eau]   {n_estran} zones d'estran retirées")

    path = out_dir / "eau.png"
    Image.fromarray(eau.astype(np.uint8)).save(path, optimize=True)
    part = (eau > 0).sum() / float(cols * rows)
    print(f"[eau] {path} écrit : {part * 100.0:.1f} % de la carte en eau, "
          f"{os.path.getsize(path) / 1024.0:.0f} ko, {time.time() - depart:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
