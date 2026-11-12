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

Les ponts en ville sont éclairés (pont de pierre à Bordeaux) : les points d'un
tronçon urbain porté par un ouvrage (position_par_rapport_au_sol >= 1) qui
tombent dans l'emprise d'un tablier de bridges.bin sont gardés sans bâtiment à
côté, et reçoivent l'altitude de la chaussée : le relief, lui, est au fond du
fleuve.

Écrit aussi lampadaires.png, le halo au sol : même grille que eau.png (un pixel
tous les PX_M mètres), un disque clair flouté par lampadaire, que terrain.frag
ajoute en lumière orangée la nuit.

Les surfaces d'aérodrome (BD TOPO) reçoivent un balisage : un feu blanc tous
les BALISE_M mètres sur leur contour, trous compris, au ras du sol, sans tri ni
halo. La BD TOPO ne distingue pas la piste des voies et des aires, alors les
seuils viennent d'OpenStreetMap (axes aeroway=runway, Overpass) : à chaque bout
de piste, une barre de feux verts en travers (seuil) et une barre rouge juste
au-delà (fin de piste), largeur OSM ou LARGEUR_PISTE_M. Les voies de
circulation restent blanches, OSM ne les sépare pas assez bien des aires.

Format de lampadaires.bin (petit-boutiste) :
    magie   : 4 octets "ALMP"
    version : uint32 (= 3 ; la 1 n'a pas l'octet de type, la 2 pas l'altitude)
    nombre  : uint32
    puis, par point : lon float32, lat float32 (degrés WGS84),
                      type uint8 (0 rue, 1 feu d'aérodrome blanc, 2 feu de pad,
                                  3 seuil vert, 4 fin de piste rouge),
                      alt float32 (altitude de la chaussée d'un pont, NaN au sol)

Usage : python3 tools/fetch_lampadaires.py [carte]   (carte par défaut : ossau)
Sortie : assets/terrain/<carte>/lampadaires.bin et lampadaires.png

Auteur : O. Booklage
Licence : GPL v2
"""

import json
import math
import os
import struct
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from terrain.config import TERRAIN_ROOT
from terrain.meta import read_meta

from foret.geometrie import en_pixels, lignes_de
from foret.reglages import NATURES_ROUTE, PX_M, WFS_LAYER_PISTE, WFS_LAYER_ROUTE
from foret.service_ign import fetch_pages, filtre_natures

PAS_M = 30.0            # un lampadaire tous les 30 m
RAYON_BATI_M = 50.0     # pas de lampadaire sans bâtiment à moins de 50 m
HALO_PX = 1.0           # rayon du halo au sol, en pixels du masque (10 m)
BALISE_M = 60.0         # un feu de bord de piste tous les 60 m (OACI)
RUE, PISTE, SEUIL, FIN = 0, 1, 3, 4   # types de point du .bin (2 : pad, moteur)
LARGEUR_PISTE_M = 30.0  # largeur de piste quand OSM ne la donne pas
PAS_SEUIL_M = 3.0       # un feu de seuil tous les 3 m (OACI)
RECUL_FIN_M = 10.0      # la barre rouge, à 10 m au-delà du seuil
OVERPASS = "https://overpass-api.de/api/interpreter"
SANS_BALISAGE = ("grass", "earth", "dirt", "sand", "gravel")   # pistes non éclairées
AU_SOL = float("nan")   # altitude d'un point posé sur le relief
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


def tabliers(path):
    """Axes des tabliers de bridges.bin (voir fetch_bridges.py) : par tablier,
       la demi-largeur maximale et ses points (lon, lat, alt)."""
    if not path.exists():
        return []
    liste = []
    with open(path, "rb") as f:
        magie, version, nombre = struct.unpack("<4sII", f.read(12))
        if magie != b"ABRG" or version != 2:
            raise RuntimeError(f"{path} : en-tête inconnu")
        for _ in range(nombre):
            gauche, droite, _, n = struct.unpack("<ffBH", f.read(11))
            liste.append((max(gauche, droite), list(struct.iter_unpack("<fff", f.read(12 * n)))))
    return liste


def altitude_sur_tablier(point, tabliers, m_par_deg_lon):
    """Altitude de la chaussée au droit du point s'il tombe dans l'emprise
       d'un tablier (projection sur son axe), AU_SOL sinon."""
    lon, lat = point
    for demi, axe in tabliers:
        for (lon0, lat0, alt0), (lon1, lat1, alt1) in zip(axe, axe[1:]):
            ax, ay = (lon1 - lon0) * m_par_deg_lon, (lat1 - lat0) * M_PAR_DEG_LAT
            px, py = (lon - lon0) * m_par_deg_lon, (lat - lat0) * M_PAR_DEG_LAT
            longueur2 = ax * ax + ay * ay
            if longueur2 == 0:
                continue
            t = max(0.0, min(1.0, (px * ax + py * ay) / longueur2))
            if math.hypot(px - t * ax, py - t * ay) <= demi:
                return alt0 + (alt1 - alt0) * t
    return AU_SOL


def tous_les_anneaux(geometrie):
    """Contours ET trous d'un polygone ou multipolygone : à Dax, toute la
       surface revêtue (piste, voies, aires) est un seul polygone, et un bord de
       la piste longe un trou d'herbe. anneaux_de (foret) ignore les trous."""
    if geometrie is None:
        return []
    if geometrie["type"] == "Polygon":
        return list(geometrie["coordinates"])
    if geometrie["type"] == "MultiPolygon":
        return [a for poly in geometrie["coordinates"] for a in poly]
    return []


def pistes_osm(bbox):
    """Axes de piste d'OpenStreetMap sur l'emprise : par piste, sa largeur (m)
       et ses points (lon, lat). Les tronçons qui se touchent sont enchaînés,
       une piste coupée en deux n'a pas de seuil au milieu. Les surfaces
       (way fermé) sont ignorées."""
    lon_min, lat_min, lon_max, lat_max = bbox.split(",")
    requete = (f'[out:json][timeout:60];way["aeroway"="runway"]'
               f'({lat_min},{lon_min},{lat_max},{lon_max});out geom;')
    # Overpass refuse (406) une requête sans User-Agent nommé.
    req = urllib.request.Request(OVERPASS, urllib.parse.urlencode({"data": requete}).encode(),
                                 {"User-Agent": "artouste (fetch_lampadaires.py)"})
    with urllib.request.urlopen(req, timeout=90) as rep:
        ways = [w for w in json.load(rep)["elements"]
                if w.get("nodes") and w["nodes"][0] != w["nodes"][-1]
                and w.get("tags", {}).get("surface") not in SANS_BALISAGE]
    # Enchaînement par noeud commun aux bouts, jusqu'à ce que plus rien ne
    # se touche : une piste en trois morceaux demande deux passes.
    chaines = [{"noeuds": list(w["nodes"]),
                "points": [(g["lon"], g["lat"]) for g in w["geometry"]],
                "deplace": [w.get("tags", {}).get("runway") == "displaced_threshold"]
                           * (len(w["nodes"]) - 1),   # un drapeau par segment
                "largeur": w.get("tags", {}).get("width")} for w in ways]

    def fusion(a, b):
        if a["noeuds"][-1] == b["noeuds"][0]:
            pass
        elif a["noeuds"][0] == b["noeuds"][-1]:
            a, b = b, a
        elif a["noeuds"][-1] == b["noeuds"][-1]:
            b = retourne(b)
        elif a["noeuds"][0] == b["noeuds"][0]:
            a = retourne(a)
        else:
            return None
        return {"noeuds": a["noeuds"] + b["noeuds"][1:],
                "points": a["points"] + b["points"][1:],
                "deplace": a["deplace"] + b["deplace"],
                "largeur": a["largeur"] or b["largeur"]}

    def retourne(c):
        return {**c, "noeuds": c["noeuds"][::-1], "points": c["points"][::-1],
                "deplace": c["deplace"][::-1]}

    fusionne = True
    while fusionne:
        fusionne = False
        for i in range(len(chaines)):
            for j in range(i + 1, len(chaines)):
                f = fusion(chaines[i], chaines[j])
                if f is not None:
                    chaines[i] = f
                    del chaines[j]
                    fusionne = True
                    break
            if fusionne:
                break
    pistes = []
    for c in chaines:
        try:
            largeur = float(str(c["largeur"]).split()[0])
        except (TypeError, ValueError):
            largeur = LARGEUR_PISTE_M
        pistes.append((largeur, c["points"], c["deplace"]))
    return pistes


def barre(bout, voisin, largeur, recul, m_par_deg_lon):
    """Feux en travers de l'axe au bout de piste, à recul mètres au-delà
       du bout (négatif : en deçà), un tous les PAS_SEUIL_M sur largeur."""
    ax = (bout[0] - voisin[0]) * m_par_deg_lon
    ay = (bout[1] - voisin[1]) * M_PAR_DEG_LAT
    longueur = math.hypot(ax, ay)
    if longueur == 0:
        return []
    ax, ay = ax / longueur, ay / longueur         # vers l'extérieur de la piste
    n = max(2, int(largeur // PAS_SEUIL_M) + 1)
    feux = []
    for k in range(n):
        d = -largeur / 2.0 + k * largeur / (n - 1)  # position en travers
        mx = bout[0] * m_par_deg_lon + ax * recul - ay * d
        my = bout[1] * M_PAR_DEG_LAT + ay * recul + ax * d
        feux.append((mx / m_par_deg_lon, my / M_PAR_DEG_LAT))
    return feux


def seuils(pistes, m_par_deg_lon):
    """(lon, lat, type) des barres de seuil (vert) et de fin (rouge) aux deux
       bouts de chaque piste. Un seuil décalé (OSM runway=displaced_threshold
       au bout) met le vert au bout intérieur de ce segment, le rouge reste au
       bout de la chaussée."""
    feux = []
    for largeur, pts, deplace in pistes:
        if len(pts) < 2:
            continue
        for sens in (1, -1):
            p = pts[::sens]
            bout, voisin = p[0], p[1]
            seuil = p[1] if deplace[::sens][0] and len(p) > 2 else p[0]
            feux += [(lon, lat, SEUIL) for lon, lat in barre(seuil, p[-1], largeur, 0.0, m_par_deg_lon)]
            feux += [(lon, lat, FIN) for lon, lat in barre(bout, voisin, largeur, RECUL_FIN_M, m_par_deg_lon)]
    return feux


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
    portes = []     # points des tronçons sur un ouvrage, à poser sur le tablier
    troncons = 0
    for feat in fetch_pages(bbox, WFS_LAYER_ROUTE,
                            "nature,urbain,position_par_rapport_au_sol,geometrie",
                            filtre=filtre_natures(bbox, NATURES_ROUTE)):
        props = feat["properties"]
        if not props.get("urbain"):
            continue
        troncons += 1
        porte = str(props.get("position_par_rapport_au_sol")).isdigit() and \
            int(props["position_par_rapport_au_sol"]) >= 1
        for ligne in lignes_de(feat.get("geometry")):
            cible = portes if porte else points
            cible += [p for p in echantillonner(ligne, PAS_M, m_par_deg_lon)
                      if lon_min <= p[0] <= lon_max and lat_min <= p[1] <= lat_max]

    poses = len(points) + len(portes)
    axes = tabliers(out_dir / "bridges.bin")
    sur_pont = [(p, alt) for p in portes
                for alt in [altitude_sur_tablier(p, axes, m_par_deg_lon)]
                if not math.isnan(alt)]
    points = pres_du_bati(points, sommets_batiments(out_dir / "buildings.bin"),
                          RAYON_BATI_M, m_par_deg_lon)
    points += [p for p, _ in sur_pont]   # un pont en ville est éclairé, bâti ou pas
    altitudes = dict(sur_pont)

    feux = [(lon, lat, PISTE)
            for feat in fetch_pages(bbox, WFS_LAYER_PISTE)
            for anneau in tous_les_anneaux(feat.get("geometry"))
            for lon, lat in echantillonner(anneau, BALISE_M, m_par_deg_lon)]
    if feux:   # un aérodrome sur la carte : ses seuils par OSM
        feux += seuils(pistes_osm(bbox), m_par_deg_lon)
    feux = [f for f in feux if lon_min <= f[0] <= lon_max and lat_min <= f[1] <= lat_max]

    path = out_dir / "lampadaires.bin"
    with open(path, "wb") as f:
        f.write(b"ALMP" + struct.pack("<II", 3, len(points) + len(feux)))
        for lon, lat in points:
            f.write(struct.pack("<ffBf", lon, lat, RUE, altitudes.get((lon, lat), AU_SOL)))
        for lon, lat, genre in feux:
            f.write(struct.pack("<ffBf", lon, lat, genre, AU_SOL))
    print(f"[lampadaires] {path} écrit : {len(points)} points gardés sur {poses} posés "
          f"({troncons} tronçons urbains, {len(sur_pont)} points sur un pont), "
          f"{len(feux)} feux d'aérodrome dont {sum(f[2] != PISTE for f in feux)} de seuil, "
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
