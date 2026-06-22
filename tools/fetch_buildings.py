#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fetch_buildings.py
Télécharge les emprises de bâtiments (avec leur hauteur réelle) sur l'emprise
d'une zone, depuis la BD TOPO de l'IGN via le service WFS de la Géoplateforme,
et les écrit en fichier binaire compact (buildings.bin) que le moteur extrude en
volumes simples (murs + toit plat).

Même producteur et même licence que le relief et l'orthophoto (IGN Géoplateforme,
Licence Ouverte Etalab 2.0). La zone (bornes géographiques) est lue dans le
dictionnaire ZONES de fetch_terrain.py, partagé avec la génération du terrain.

Filtre par zone : on écarte les bâtiments plus bas que le seuil de la zone (clé
"height_min" de ZONES, HEIGHT_MIN par défaut). En ville ce seuil écarte les cabanes
et abris pour ne pas alourdir la scène ; en montagne il est abaissé à 0 pour garder
les cabanes et bergeries, utiles au repérage.

Format de buildings.bin (petit-boutiste) :
    magie   : 4 octets "ABLD"
    version : uint32 (= 1)
    nombre  : uint32 (nombre de bâtiments)
    puis, pour chaque bâtiment :
        hauteur : float32 (mètres)
        n       : uint16  (nombre de sommets de l'emprise, anneau non refermé)
        n x (lon : float32, lat : float32)   (degrés WGS84)

Données : IGN Géoplateforme (BD TOPO), Licence Ouverte Etalab 2.0.
Dépendances : Python 3 (bibliothèque standard seulement).

Usage : python3 tools/fetch_buildings.py [zone]   (zone par défaut : ossau)
Sortie : assets/terrain/<zone>/buildings.bin

Auteur : O. Booklage
Licence : GPL v2
"""

import os
import struct
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import json

# On réutilise la description des zones (bornes, dossier de sortie) du paquet
# terrain : une seule source de vérité pour l'emprise géographique.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from terrain.zones import ZONES, DEFAULT_ZONE  # noqa: E402
from terrain.config import TERRAIN_ROOT  # noqa: E402

# --- Service WFS BD TOPO -----------------------------------------------------
WFS_URL = "https://data.geopf.fr/wfs/ows"
WFS_LAYER = "BDTOPO_V3:batiment"

# Nombre de bâtiments par requête. Le service en sert de gros volumes ; on pagine
# en avançant de l'effectif réellement renvoyé (et non du nombre demandé), au cas
# où le service plafonnerait une page.
PAGE = 5000

# Hauteur minimale retenue par défaut (mètres). En dessous : cabanes, abris,
# garages isolés, qu'on écarte en ville pour ne pas alourdir la scène de bruit.
# Une zone peut abaisser ce seuil via sa clé "height_min" (en montagne, on garde
# les cabanes et bergeries, peu nombreuses et utiles au repérage).
HEIGHT_MIN = 2.0

# Hauteur de rendu minimale (mètres) : un bâtiment plus bas (cabane, ou sans
# hauteur renseignée) est extrudé au moins à cette valeur pour rester visible.
MIN_RENDER_HEIGHT = 2.0

# En-tête du fichier binaire.
MAGIC = b"ABLD"
VERSION = 1


def fetch_page(bbox, start):
    """Récupère une page de bâtiments (hauteur + géométrie) à partir de start."""
    params = {
        "SERVICE": "WFS",
        "VERSION": "2.0.0",
        "REQUEST": "GetFeature",
        "TYPENAMES": WFS_LAYER,
        "SRSNAME": "EPSG:4326",
        "BBOX": bbox + ",EPSG:4326",
        "OUTPUTFORMAT": "application/json",
        "PROPERTYNAME": "hauteur,geometrie",  # géométrie + hauteur seules : charge réduite
        "COUNT": str(PAGE),
        "STARTINDEX": str(start),
    }
    url = WFS_URL + "?" + urllib.parse.urlencode(params)
    last_err = None
    for attempt in range(6):
        try:
            with urllib.request.urlopen(url, timeout=120) as resp:
                return json.loads(resp.read())["features"]
        except urllib.error.HTTPError as err:
            last_err = err
            time.sleep((5.0 if err.code == 429 else 2.0) * (attempt + 1))
        except Exception as err:  # réseau capricieux : on retente
            last_err = err
            time.sleep(1.0 + attempt)
    raise RuntimeError(f"page de bâtiments (start={start}) : échec ({last_err})")


def rings_of(geometry):
    """Renvoie la liste des anneaux extérieurs (un par polygone) d'une géométrie
       Polygon ou MultiPolygon. Les trous (anneaux intérieurs) sont ignorés."""
    if geometry is None:
        return []
    gtype = geometry["type"]
    coords = geometry["coordinates"]
    if gtype == "Polygon":
        return [coords[0]]
    if gtype == "MultiPolygon":
        return [poly[0] for poly in coords]
    return []


def write_buildings(path, buildings):
    """Écrit la liste de bâtiments (hauteur, anneau de (lon, lat)) en binaire."""
    with open(path, "wb") as out:
        out.write(MAGIC)
        out.write(struct.pack("<II", VERSION, len(buildings)))
        for height, ring in buildings:
            out.write(struct.pack("<fH", height, len(ring)))
            for lon, lat in ring:
                out.write(struct.pack("<ff", lon, lat))


def main():
    zone = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ZONE
    if zone not in ZONES:
        connues = ", ".join(sorted(ZONES))
        raise RuntimeError(f"zone inconnue : {zone} (zones connues : {connues})")

    lon_min, lon_max, lat_min, lat_max = ZONES[zone]["bbox"]
    bbox = f"{lon_min},{lat_min},{lon_max},{lat_max}"
    out_dir = os.path.join(TERRAIN_ROOT, zone)
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "buildings.bin")

    # Seuil de hauteur propre à la zone (défaut HEIGHT_MIN). En montagne, on l'abaisse
    # pour garder les cabanes et bergeries.
    height_min = ZONES[zone].get("height_min", HEIGHT_MIN)

    print(f"[batiments] zone {zone} : WFS BD TOPO sur {bbox} (hauteur >= {height_min} m)")
    start = time.time()

    buildings = []   # liste de (hauteur, anneau [(lon, lat), ...])
    examined = 0
    index = 0
    while True:
        feats = fetch_page(bbox, index)
        if not feats:
            break
        for f in feats:
            examined += 1
            # Hauteur BD TOPO (parfois absente : on la traite comme 0 pour le filtre).
            height = f["properties"].get("hauteur")
            h = float(height) if height is not None else 0.0
            if h < height_min:
                continue
            # Cabane ou hauteur manquante : on extrude au moins MIN_RENDER_HEIGHT pour
            # qu'elle reste visible (sans effet sur les bâtiments déjà plus hauts).
            h = max(h, MIN_RENDER_HEIGHT)
            for ring in rings_of(f.get("geometry")):
                # L'anneau GeoJSON est refermé (dernier point = premier) : on retire
                # le doublon, le moteur referme l'emprise.
                pts = [(round(p[0], 6), round(p[1], 6)) for p in ring]
                if len(pts) >= 2 and pts[0] == pts[-1]:
                    pts = pts[:-1]
                if len(pts) >= 3:
                    buildings.append((h, pts))
        index += len(feats)
        print(f"[batiments]   {index} examines, {len(buildings)} retenus...")

    write_buildings(path, buildings)
    size_mo = os.path.getsize(path) / (1024 * 1024)
    print(f"[batiments] {path} ecrit : {len(buildings)} batiments "
          f"(sur {examined} examines, filtre hauteur >= {height_min} m), "
          f"{size_mo:.1f} Mo")
    print(f"[ok] termine en {time.time() - start:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
