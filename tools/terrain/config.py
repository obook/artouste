# -*- coding: utf-8 -*-
"""
config.py
Réglages partagés du téléchargeur de terrain : services IGN, paramètres de grille
et réglages de la zone choisie. Ces derniers sont des variables de module mises à
jour par select_zone() et lues par les autres modules sous la forme config.X (par
exemple config.LON_MIN). On garde ainsi un état global unique et simple, comme
dans la version d'origine.

Auteur : O. Booklage
Licence : GPL v2
"""

import os

from terrain.zones import ZONES

# --- Services IGN ------------------------------------------------------------
ALTI_URL = "https://data.geopf.fr/altimetrie/1.0/calcul/alti/rest/elevation.json"
ALTI_RESOURCE = "ign_rge_alti_wld"
WMS_URL = "https://data.geopf.fr/wms-r/wms"
WMS_LAYER = "ORTHOIMAGERY.ORTHOPHOTOS"

# Valeur renvoyée par l'API là où il n'y a pas de donnée terrestre (mer) ; on la
# ramène au niveau de la mer (0 m).
NODATA = -1000.0

# Nombre de points de la grille d'altitude (et de sommets du maillage).
# Défaut : 512 ; surcharge possible par la clé "grid" de la zone.
COLS = 512  # axe ouest -> est (longitude)
ROWS = 512  # axe nord -> sud (latitude)

# Taille de l'orthophoto téléchargée (le moteur la drape sur le maillage).
# Défaut : 2048 ; surcharge possible par la clé "ortho_px" de la zone.
ORTHO_HEIGHT = 2048

# Couleur de repli de la mer si l'on ne trouve pas assez de pixels d'eau
# photographiés pour la mesurer. La vraie couleur est échantillonnée sur la mer
# de la photo (voir fetch_ortho) ; elle doit rester proche du plan de mer du
# moteur (SEA_COLOR dans Application.cpp).
SEA_FALLBACK = (43, 65, 70)

# Nombre maximal de points par requête : au-delà, l'URL de l'API dépasse sa limite
# de longueur (erreur HTTP 414). On découpe donc une rangée en plusieurs morceaux.
MAX_PTS_PER_REQUEST = 200

# Largeur/hauteur maximale d'une seule requête WMS GetMap (limite serveur IGN
# ~5010 px). Au-delà, fetch_ortho() découpe l'emprise en tuiles et les assemble.
WMS_MAX_PX = 5000

# Racine des terrains : chaque zone est rangée dans un sous-dossier portant son nom.
TERRAIN_ROOT = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "terrain")

# --- Réglages de la zone choisie (fixés par select_zone) ---------------------
# Emprise géographique (WGS84), recoloration de la mer, point de départ du vol,
# libellé, lieux remarquables et dossier de sortie. Valeurs renseignées au
# lancement à partir de l'entrée ZONES sélectionnée.
LON_MIN, LON_MAX = 0.0, 0.0
LAT_MIN, LAT_MAX = 0.0, 0.0
RECOLOR_SEA = False
START_LON, START_LAT = 0.0, 0.0
START_HEADING = 90.0  # cap initial (deg boussole) ; 90 = est, l'orientation identité
ZONE_TITLE = ""
ZONE_LANDMARKS = []
ZONE_HELIPADS = []
ZONE_EXCLUSIONS = []
ZONE_HAPI = []
OUT_DIR = ""


def select_zone(name):
    """Fixe les réglages globaux (emprise, mer, départ, sortie) pour la zone donnée."""
    global LON_MIN, LON_MAX, LAT_MIN, LAT_MAX, RECOLOR_SEA, START_LON, START_LAT
    global START_HEADING
    global ZONE_TITLE, ZONE_LANDMARKS, ZONE_HELIPADS, ZONE_EXCLUSIONS, ZONE_HAPI, OUT_DIR
    global COLS, ROWS, ORTHO_HEIGHT
    if name not in ZONES:
        connues = ", ".join(sorted(ZONES))
        raise RuntimeError(f"zone inconnue : {name} (zones connues : {connues})")
    zone = ZONES[name]
    LON_MIN, LON_MAX, LAT_MIN, LAT_MAX = zone["bbox"]
    RECOLOR_SEA = zone["recolor_sea"]
    START_LON, START_LAT = zone["start"]
    START_HEADING = float(zone.get("start_heading", 90.0))
    ZONE_TITLE = zone["title"]
    ZONE_LANDMARKS = zone["landmarks"]
    ZONE_HELIPADS = zone.get("helipads", [])
    ZONE_EXCLUSIONS = zone.get("exclusions", [])
    ZONE_HAPI = zone.get("hapi", [])
    OUT_DIR = os.path.join(TERRAIN_ROOT, name)
    COLS = ROWS = int(zone.get("grid", 512))
    ORTHO_HEIGHT = int(zone.get("ortho_px", 2048))
