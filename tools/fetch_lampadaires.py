#!/usr/bin/env python3
"""
fetch_lampadaires.py
Fabrique les points d'éclairage public d'une carte (lampadaires.bin) : vu
d'hélicoptère, une ville la nuit est un chapelet de points orangés qui dessine
les rues. On pose un point tous les PAS_M mètres le long des tronçons de route
URBAINS de la BD TOPO (champ "urbain", Géoplateforme IGN, Licence Ouverte
Etalab 2.0), et on ne garde que ceux qui ont un bâtiment (buildings.bin) à
moins de RAYON_BATI_M : une route "urbaine" qui traverse les champs reste
noire. Le moteur les dessine en points lumineux la nuit (render/Lampadaires).

Écrit aussi lampadaires.png, le halo au sol : même grille que eau.png (un pixel
tous les PX_M mètres), un disque clair flouté par lampadaire, que terrain.frag
ajoute en lumière orangée la nuit.

Les surfaces d'aérodrome (BD TOPO) reçoivent un balisage : un feu blanc tous
les BALISE_M mètres sur leur contour, au ras du sol, sans tri ni halo. L'OACI
veut aussi des seuils verts, des extrémités rouges et des voies de circulation
bleues, mais la BD TOPO découpe pistes, voies et aires en polygones sans
distinction : tout est blanc.

Format de lampadaires.bin (petit-boutiste) :
    magie   : 4 octets "ALMP"
    version : uint32 (= 2 ; la version 1 n'a pas l'octet de type)
    nombre  : uint32
    puis, par point : lon float32, lat float32 (degrés WGS84),
                      type uint8 (0 lampadaire de rue, 1 feu d'aérodrome)

Usage : python3 tools/fetch_lampadaires.py [carte]   (carte par défaut : ossau)
Sortie : assets/terrain/<carte>/lampadaires.bin et lampadaires.png

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import os
import struct
import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from terrain.config import TERRAIN_ROOT
from terrain.meta import read_meta

from foret.geometrie import anneaux_de, en_pixels, lignes_de
from foret.reglages import NATURES_ROUTE, PX_M, WFS_LAYER_PISTE, WFS_LAYER_ROUTE
from foret.service_ign import fetch_pages, filtre_natures

PAS_M = 30.0            # un lampadaire tous les 30 m
RAYON_BATI_M = 50.0     # pas de lampadaire sans bâtiment à moins de 50 m
HALO_PX = 1.0           # rayon du halo au sol, en pixels du masque (10 m)
BALISE_M = 60.0         # un feu de bord de piste tous les 60 m (OACI)
RUE, PISTE = 0, 1       # types de point du .bin
M_PAR_DEG_LAT = 111320.0


def sommets_batiments(path):
    """Tous les sommets (lon, lat) des emprises de buildings.bin (voir
       fetch_buildings.py) ; vide si la carte n'a pas de bâtiments."""
    if not path.exists():
        return []
    sommets = []
    with open(path, "rb") as f:
        magie, version, nombre = struct.unpack("<4sII", f.read(12))
        if magie != b"ABLD" or version != 2:
            raise RuntimeError(f"{path} : en-tête inconnu")
        for _ in range(nombre):
            n = struct.unpack("<ffBH", f.read(11))[3]
            sommets += struct.iter_unpack("<ff", f.read(8 * n))
    return sommets


def pres_du_bati(points, sommets, rayon, m_par_deg_lon):
    """Les points qui ont un sommet de bâtiment à moins de rayon mètres :
       les sommets sont rangés par case de rayon x rayon, on ne compare un
       point qu'aux neuf cases autour de lui."""
    def case(lon, lat):
        return (int(lon * m_par_deg_lon // rayon), int(lat * M_PAR_DEG_LAT // rayon))
    cases = {}
    for lon, lat in sommets:
        cases.setdefault(case(lon, lat), []).append((lon, lat))
    garde = []
    for lon, lat in points:
        cx, cy = case(lon, lat)
        voisins = (s for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                   for s in cases.get((cx + dx, cy + dy), ()))
        if any(math.hypot((s[0] - lon) * m_par_deg_lon, (s[1] - lat) * M_PAR_DEG_LAT) <= rayon
               for s in voisins):
            garde.append((lon, lat))
    return garde


def halo_au_sol(points, cadre):
    """Masque du halo : un disque de HALO_PX par lampadaire, flouté."""
    image = Image.new("L", (cadre[4], cadre[5]), 0)
    dessin = ImageDraw.Draw(image)
    for x, y in en_pixels(points, cadre):
        dessin.ellipse((x - HALO_PX, y - HALO_PX, x + HALO_PX, y + HALO_PX), fill=255)
    return image.filter(ImageFilter.GaussianBlur(0.7))


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
    cols = max(1, round(float(meta["width_m"]) / PX_M))
    rows = max(1, round(float(meta["height_m"]) / PX_M))
    cadre = (lon_min, lon_max, lat_min, lat_max, cols, rows)
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

    poses = len(points)
    points = pres_du_bati(points, sommets_batiments(out_dir / "buildings.bin"),
                          RAYON_BATI_M, m_par_deg_lon)

    feux = [(lon, lat)
            for feat in fetch_pages(bbox, WFS_LAYER_PISTE)
            for anneau in anneaux_de(feat.get("geometry"))
            for lon, lat in echantillonner(anneau, BALISE_M, m_par_deg_lon)
            if lon_min <= lon <= lon_max and lat_min <= lat <= lat_max]

    path = out_dir / "lampadaires.bin"
    with open(path, "wb") as f:
        f.write(b"ALMP" + struct.pack("<II", 2, len(points) + len(feux)))
        for genre, liste in ((RUE, points), (PISTE, feux)):
            for lon, lat in liste:
                f.write(struct.pack("<ffB", lon, lat, genre))
    print(f"[lampadaires] {path} écrit : {len(points)} points gardés sur {poses} posés "
          f"({troncons} tronçons urbains), {len(feux)} feux d'aérodrome, "
          f"{os.path.getsize(path) / 1024.0:.0f} ko, {time.time() - depart:.0f} s")

    path = out_dir / "lampadaires.png"
    halo_au_sol(points, cadre).save(path, optimize=True)
    print(f"[lampadaires] {path} écrit : masque {cols}x{rows} ({PX_M:.0f} m/px), "
          f"{os.path.getsize(path) / 1024.0:.0f} ko")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
