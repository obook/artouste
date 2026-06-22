# -*- coding: utf-8 -*-
"""
outputs.py
Écriture des fichiers de sortie lus par le moteur : le calage du terrain
(terrain.txt), les lieux remarquables (landmarks.txt) et les hélipads
(helipads.txt). Tous au format texte simple, une entrée par ligne.

Auteur : O. Booklage
Licence : GPL v2
"""

import os

from terrain import config


def write_metadata(elev_min, elev_max, width_m, height_m, ortho_w, start_x, start_z):
    """Écrit le fichier de calage lu par le moteur (clés simples, une par ligne)."""
    path = os.path.join(config.OUT_DIR, "terrain.txt")
    with open(path, "w", encoding="utf-8") as out:
        out.write(f"# Terrain Artouste - {config.ZONE_TITLE}\n")
        out.write("# Données IGN Géoplateforme (RGE ALTI + BD ORTHO), Licence Ouverte Etalab 2.0\n")
        out.write("# width_m / height_m : dimensions au sol du maillage, en mètres\n")
        out.write(f"cols {config.COLS}\n")
        out.write(f"rows {config.ROWS}\n")
        out.write(f"width_m {width_m:.1f}\n")
        out.write(f"height_m {height_m:.1f}\n")
        out.write(f"elev_min {elev_min:.2f}\n")
        out.write(f"elev_max {elev_max:.2f}\n")
        out.write(f"lon_min {config.LON_MIN}\n")
        out.write(f"lon_max {config.LON_MAX}\n")
        out.write(f"lat_min {config.LAT_MIN}\n")
        out.write(f"lat_max {config.LAT_MAX}\n")
        out.write(f"ortho_width {ortho_w}\n")
        out.write(f"ortho_height {config.ORTHO_HEIGHT}\n")
        # Plan de mer du moteur : oui en bord de mer, non en montagne.
        out.write(f"sea {1 if config.RECOLOR_SEA else 0}\n")
        # Point de départ (replat) en coordonnées monde (X est, Z sud).
        out.write(f"start_x {start_x:.1f}\n")
        out.write(f"start_z {start_z:.1f}\n")
    print(f"[meta] {path} écrit")


def write_landmarks():
    """Écrit les lieux remarquables de la zone (un par ligne : lon lat nom)."""
    path = os.path.join(config.OUT_DIR, "landmarks.txt")
    with open(path, "w", encoding="utf-8") as out:
        out.write(f"# Lieux remarquables - {config.ZONE_TITLE} (un par ligne : lon lat nom)\n")
        out.write("# Le nom est le reste de la ligne et peut contenir des espaces et des accents.\n")
        for name, lon, lat in config.ZONE_LANDMARKS:
            out.write(f"{lon} {lat} {name}\n")
    print(f"[lieux] {path} écrit ({len(config.ZONE_LANDMARKS)} lieu(x))")


def write_helipads():
    """Écrit les hélipads de la zone (un par ligne : lon lat nom). Aucun fichier si
       la zone n'en déclare pas (l'hélipad de départ est géré à part par le moteur)."""
    if not config.ZONE_HELIPADS:
        return
    path = os.path.join(config.OUT_DIR, "helipads.txt")
    with open(path, "w", encoding="utf-8") as out:
        out.write(f"# Helipads - {config.ZONE_TITLE} (un par ligne : lon lat nom)\n")
        out.write("# Positions approximatives, à affiner sur place.\n")
        for name, lon, lat in config.ZONE_HELIPADS:
            out.write(f"{lon} {lat} {name}\n")
    print(f"[helipads] {path} écrit ({len(config.ZONE_HELIPADS)} helipad(s))")
