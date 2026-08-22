#!/usr/bin/env python3
"""
fetch_bridges.py
Télécharge les ouvrages d'art (ponts, viaducs, passages supérieurs) d'une zone
depuis la BD TOPO de l'IGN via le service WFS de la Géoplateforme, et les écrit
en fichier binaire compact (bridges.bin) que le moteur extrude en tabliers.

Pourquoi ce fichier : le relief vient de RGE ALTI, un modèle numérique de
TERRAIN. Il donne le sol nu, sans ouvrage d'art. Un pont n'existe donc que dans
l'orthophoto plaquée dessus, et son tablier suit le creux du lit du fleuve : il
plonge dans l'eau. Mesuré sur le pont d'Empalot à Toulouse, 8,5 m de creux entre
la berge et le milieu de la Garonne.

Ce que la BD TOPO apporte que le relief n'a pas : les tronçons de route et de
voie ferrée portent une géométrie en TROIS dimensions, dont l'altitude du
tablier, et un attribut position_par_rapport_au_sol qui vaut 1 ou plus quand la
voie est portée par un ouvrage. C'est exactement ce qu'il faut pour poser un
ruban à la bonne hauteur.

Format de bridges.bin (petit-boutiste) :
    magie   : 4 octets "ABRG"
    version : uint32 (= 1)
    nombre  : uint32 (nombre de tabliers)
    puis, pour chaque tablier :
        largeur : float32 (mètres)
        n       : uint16  (nombre de points de l'axe, au moins 2)
        n x (lon : float32, lat : float32, altitude : float32)

Données : IGN Géoplateforme (BD TOPO), Licence Ouverte Etalab 2.0.
Dépendances : Python 3 (bibliothèque standard seulement).

Usage : python3 tools/fetch_bridges.py [zone]   (zone par défaut : ossau)
Sortie : assets/terrain/<zone>/bridges.bin

Auteur : O. Booklage
Licence : GPL v2
"""

import json
import os
import struct
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

# On réutilise la description des zones (bornes, dossier de sortie) du paquet
# terrain : une seule source de vérité pour l'emprise géographique.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from terrain.zones import ZONES, DEFAULT_ZONE
from terrain.config import TERRAIN_ROOT

WFS_URL = "https://data.geopf.fr/wfs/ows"

# Tronçons par requête. Les ouvrages d'art sont rares : une seule page suffit
# souvent, mais on pagine quand même pour les agglomérations à échangeurs.
PAGE = 1000

# Largeur de repli quand la BD TOPO ne renseigne pas la chaussée (mètres).
LARGEUR_ROUTE_DEFAUT = 6.0

# Largeur d'une voie ferrée : l'entraxe usuel entre deux voies fait 4 m, et le
# ballast déborde de part et d'autre. La BD TOPO ne donne que le NOMBRE de
# voies, pas une largeur en mètres.
LARGEUR_PAR_VOIE = 4.5
LARGEUR_RAIL_MIN = 5.0

# Un tablier plus étroit que ça ne se voit pas d'en haut et ne vaut pas ses
# sommets : passerelles piétonnes de service, escaliers.
LARGEUR_MIN = 3.0

MAGIC = b"ABRG"
VERSION = 1


def demander(couche, proprietes, bbox, start):
    """Une page de tronçons portés par un ouvrage. Le filtre CQL écarte les
       tronçons au sol côté serveur : sans lui il faudrait rapatrier tout le
       réseau routier de l'emprise pour n'en garder qu'un millième.
       ATTENTION : le BBOX de CQL attend lon, lat dans cet ordre, contrairement
       au paramètre BBOX de WFS 2.0 en EPSG:4326."""
    lon_min, lon_max, lat_min, lat_max = bbox
    params = {
        "SERVICE": "WFS",
        "VERSION": "2.0.0",
        "REQUEST": "GetFeature",
        "TYPENAMES": couche,
        "SRSNAME": "EPSG:4326",
        "OUTPUTFORMAT": "application/json",
        "PROPERTYNAME": proprietes,
        "COUNT": str(PAGE),
        "STARTINDEX": str(start),
        "CQL_FILTER": (f"position_par_rapport_au_sol>0 AND "
                       f"BBOX(geometrie,{lon_min},{lat_min},{lon_max},{lat_max},'EPSG:4326')"),
    }
    url = WFS_URL + "?" + urllib.parse.urlencode(params)
    derniere = None
    for essai in range(6):
        try:
            with urllib.request.urlopen(url, timeout=120) as resp:
                return json.loads(resp.read())["features"]
        except urllib.error.HTTPError as err:
            derniere = err
            time.sleep((5.0 if err.code == 429 else 2.0) * (essai + 1))
        except Exception as err:  # réseau capricieux : on retente
            derniere = err
            time.sleep(1.0 + essai)
    raise RuntimeError(f"page de {couche} (start={start}) : échec ({derniere})")


def lignes_de(geometrie):
    """Liste des lignes d'une géométrie LineString ou MultiLineString."""
    if geometrie is None:
        return []
    if geometrie["type"] == "LineString":
        return [geometrie["coordinates"]]
    if geometrie["type"] == "MultiLineString":
        return geometrie["coordinates"]
    return []


def tabliers_de(feature, largeur):
    """Découpe la géométrie d'un tronçon en tabliers exploitables. Un point sans
       altitude rend la ligne inutilisable : c'est la seule chose que le relief
       ne sait pas déjà."""
    tabliers = []
    for ligne in lignes_de(feature.get("geometry")):
        points = [(round(p[0], 6), round(p[1], 6), round(p[2], 2))
                  for p in ligne if len(p) >= 3]
        if len(points) != len(ligne) or len(points) < 2:
            continue
        # Le format ne porte qu'un compteur 16 bits, et un tablier de plus de
        # 65535 points n'existe pas : garde-fou, pas un cas réel.
        if len(points) > 65535:
            points = points[:65535]
        tabliers.append((largeur, points))
    return tabliers


def largeur_route(proprietes):
    valeur = proprietes.get("largeur_de_chaussee")
    try:
        largeur = float(valeur)
    except (TypeError, ValueError):
        return LARGEUR_ROUTE_DEFAUT
    return largeur if largeur > 0.0 else LARGEUR_ROUTE_DEFAUT


def largeur_rail(proprietes):
    try:
        voies = int(proprietes.get("nombre_de_voies"))
    except (TypeError, ValueError):
        voies = 1
    return max(LARGEUR_RAIL_MIN, LARGEUR_PAR_VOIE * max(1, voies))


def collecter(couche, proprietes, bbox, largeur_de):
    """Toutes les pages d'une couche, converties en tabliers."""
    tabliers = []
    examines = 0
    index = 0
    while True:
        feats = demander(couche, proprietes, bbox, index)
        if not feats:
            break
        for f in feats:
            examines += 1
            largeur = largeur_de(f["properties"])
            if largeur < LARGEUR_MIN:
                continue
            tabliers.extend(tabliers_de(f, largeur))
        index += len(feats)
        print(f"[ponts]   {couche} : {index} examinés, {len(tabliers)} tabliers...")
    return tabliers, examines


def ecrire(chemin, tabliers):
    with open(chemin, "wb") as out:
        out.write(MAGIC)
        out.write(struct.pack("<II", VERSION, len(tabliers)))
        for largeur, points in tabliers:
            out.write(struct.pack("<fH", largeur, len(points)))
            for lon, lat, alt in points:
                out.write(struct.pack("<fff", lon, lat, alt))


def main():
    zone = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ZONE
    if zone not in ZONES:
        connues = ", ".join(sorted(ZONES))
        raise RuntimeError(f"zone inconnue : {zone} (zones connues : {connues})")

    bbox = ZONES[zone]["bbox"]
    dossier = os.path.join(TERRAIN_ROOT, zone)
    os.makedirs(dossier, exist_ok=True)
    chemin = os.path.join(dossier, "bridges.bin")

    print(f"[ponts] zone {zone} : WFS BD TOPO sur {bbox}")
    debut = time.time()

    routes, vus_routes = collecter("BDTOPO_V3:troncon_de_route",
                                   "largeur_de_chaussee,position_par_rapport_au_sol,geometrie",
                                   bbox, largeur_route)
    rails, vus_rails = collecter("BDTOPO_V3:troncon_de_voie_ferree",
                                 "nombre_de_voies,position_par_rapport_au_sol,geometrie",
                                 bbox, largeur_rail)

    tabliers = routes + rails
    ecrire(chemin, tabliers)
    ko = os.path.getsize(chemin) / 1024.0
    print(f"[ponts] {chemin} écrit : {len(tabliers)} tabliers "
          f"({len(routes)} routiers sur {vus_routes} tronçons, "
          f"{len(rails)} ferroviaires sur {vus_rails}), {ko:.0f} ko")
    print(f"[ok] terminé en {time.time() - debut:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
