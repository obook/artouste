#!/usr/bin/env python3
"""
fetch_buildings.py
Télécharge les emprises de bâtiments (avec leur hauteur réelle) sur l'emprise
d'une zone, depuis la BD TOPO de l'IGN via le service WFS de la Géoplateforme,
et les écrit en fichier binaire compact (buildings.bin) que le moteur extrude en
volumes simples (murs + toit à deux pans ou plat).

Même producteur et même licence que le relief et l'orthophoto (IGN Géoplateforme,
Licence Ouverte Etalab 2.0). La zone (bornes géographiques) est lue dans le
dictionnaire ZONES de fetch_terrain.py, partagé avec la génération du terrain.

Filtre par zone : on écarte les bâtiments plus bas que le seuil de la zone (clé
"height_min" de ZONES, HEIGHT_MIN par défaut). En ville ce seuil écarte les cabanes
et abris pour ne pas alourdir la scène ; en montagne il est abaissé à 0 pour garder
les cabanes et bergeries, utiles au repérage.

Format de buildings.bin (petit-boutiste) :
    magie   : 4 octets "ABLD"
    version : uint32 (= 2 ; le moteur lit encore la version 1, sans toit)
    nombre  : uint32 (nombre de bâtiments)
    puis, pour chaque bâtiment :
        hauteur  : float32 (mètres, du sol à la gouttière)
        toit     : float32 (mètres, de la gouttière au faîtage ; -1 si inconnu)
        materiau : uint8   (premier chiffre du code BD TOPO de la toiture :
                            1 tuiles, 2 ardoises, 3 zinc..., 0 inconnu)
        n        : uint16  (nombre de sommets de l'emprise, anneau non refermé)
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
from terrain.zones import ZONES, DEFAULT_ZONE
from terrain.config import TERRAIN_ROOT

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
VERSION = 2


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
        # Champs utiles seuls : charge réduite.
        "PROPERTYNAME": "hauteur,altitude_minimale_toit,altitude_maximale_toit,"
                        "materiaux_de_la_toiture,geometrie",
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


def surface_signee(ring):
    """Surface signée d'un anneau (formule du lacet) : son signe donne le sens
       de parcours."""
    return 0.5 * sum(x0 * y1 - x1 * y0
                     for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1]))


def _se_coupent(a, b, c, d):
    """Vrai si les segments [a, b] et [c, d] se croisent franchement (une
       extrémité commune ne compte pas)."""
    if a in (c, d) or b in (c, d):
        return False

    def cote(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])

    return (cote(a, b, c) * cote(a, b, d) < 0) and (cote(c, d, a) * cote(c, d, b) < 0)


def relier_trous(contour, trous):
    """Un seul anneau pour un polygone troué : chaque trou est relié au contour
       par un pont aller-retour, parcouru en sens inverse du contour. Le moteur
       n'extrude que des anneaux simples ; ainsi le toit d'une arène ou d'un
       bâtiment sur cour s'arrête au bord de la cour, et le mur de la cour est
       monté comme les autres. Le pont, lui, passe dans la masse du bâtiment."""
    if surface_signee(contour) < 0:
        contour = contour[::-1]
    anneau = list(contour)
    trous = [t[::-1] if surface_signee(t) > 0 else list(t) for t in trous]
    # Le trou le plus à l'est d'abord : son pont ne peut pas croiser les suivants.
    trous.sort(key=lambda t: -max(p[0] for p in t))
    for k, trou in enumerate(trous):
        m = max(range(len(trou)), key=lambda i: trou[i][0])
        h = trou[m]
        aretes = list(zip(anneau, anneau[1:] + anneau[:1]))
        for t in trous[k:]:
            aretes += list(zip(t, t[1:] + t[:1]))
        candidats = sorted(range(len(anneau)),
                           key=lambda i: (anneau[i][0] - h[0]) ** 2 + (anneau[i][1] - h[1]) ** 2)
        for i in candidats:
            if not any(_se_coupent(anneau[i], h, c, d) for c, d in aretes):
                anneau[i + 1:i + 1] = trou[m:] + trou[:m] + [h, anneau[i]]
                break
        # Sans pont possible (emprise aberrante), le trou est ignoré.
    return anneau


def rings_of(geometry):
    """Renvoie un anneau par polygone d'une géométrie Polygon ou MultiPolygon,
       ses trous reliés au contour (voir relier_trous). L'anneau GeoJSON
       refermé (dernier point = premier) perd son doublon."""
    if geometry is None:
        return []
    gtype = geometry["type"]
    coords = geometry["coordinates"]
    if gtype == "Polygon":
        polygones = [coords]
    elif gtype == "MultiPolygon":
        polygones = coords
    else:
        return []
    anneaux = []
    for poly in polygones:
        ouverts = []
        for ring in poly:
            pts = [(round(p[0], 6), round(p[1], 6)) for p in ring]
            if len(pts) >= 2 and pts[0] == pts[-1]:
                pts = pts[:-1]
            if len(pts) >= 3:
                ouverts.append(pts)
        if ouverts:
            anneaux.append(relier_trous(ouverts[0], ouverts[1:]))
    return anneaux


def toit_de(props):
    """Hauteur du toit (gouttière -> faîtage, -1 si inconnue) et premier chiffre
       du code de matériau de la toiture (0 si inconnu)."""
    bas = props.get("altitude_minimale_toit")
    haut = props.get("altitude_maximale_toit")
    toit = haut - bas if bas is not None and haut is not None else -1.0
    code = props.get("materiaux_de_la_toiture") or ""
    materiau = int(code[0]) if code[:1].isdigit() else 0
    return toit, materiau


def write_buildings(path, buildings):
    """Écrit la liste de bâtiments (hauteur, toit, matériau, anneau de
       (lon, lat)) en binaire."""
    with open(path, "wb") as out:
        out.write(MAGIC)
        out.write(struct.pack("<II", VERSION, len(buildings)))
        for height, toit, materiau, ring in buildings:
            out.write(struct.pack("<ffBH", height, toit, materiau, len(ring)))
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

    print(f"[bâtiments] zone {zone} : WFS BD TOPO sur {bbox} (hauteur >= {height_min} m)")
    start = time.time()

    buildings = []   # liste de (hauteur, toit, matériau, anneau [(lon, lat), ...])
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
            toit, materiau = toit_de(f["properties"])
            for pts in rings_of(f.get("geometry")):
                buildings.append((h, toit, materiau, pts))
        index += len(feats)
        print(f"[bâtiments]   {index} examinés, {len(buildings)} retenus...")

    write_buildings(path, buildings)
    size_mo = os.path.getsize(path) / (1024 * 1024)
    print(f"[bâtiments] {path} écrit : {len(buildings)} bâtiments "
          f"(sur {examined} examinés, filtre hauteur >= {height_min} m), "
          f"{size_mo:.1f} Mo")
    print(f"[ok] terminé en {time.time() - start:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
