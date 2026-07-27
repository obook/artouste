#!/usr/bin/env python3
"""
fetch_terrain.py
Télécharge les données d'un terrain réel d'Artouste depuis la Géoplateforme IGN.
Plusieurs zones sont décrites dans le module terrain.zones ; on en choisit une au
lancement (par défaut "ossau"). Pour chaque zone, le script produit :

  - le relief, échantillonné sur une grille via l'API altimétrie (RGE ALTI),
    enregistré en carte d'altitude PNG 16 bits (heightmap.png) ;
  - l'orthophoto (BD ORTHO) via le service WMS, enregistrée en ortho.jpg ;
  - les métadonnées de calage (terrain.txt) lues par le moteur ;
  - les lieux remarquables (landmarks.txt) et les hélipads (helipads.txt).

Le détail est réparti dans le paquet terrain/ (zones, config, relief, ortho,
outputs) pour garder chaque fichier court et lisible.

Données : IGN Géoplateforme, Licence Ouverte Etalab 2.0.
Dépendances : Python 3, Pillow (PIL), NumPy, SciPy. Aucun GDAL requis.

Usage : python3 tools/fetch_terrain.py [zone] [--relief-seul]
        (zone par défaut : ossau)
Sortie : assets/terrain/<zone>/{heightmap.png, ortho.jpg, terrain.txt, landmarks.txt}

--relief-seul ne refait QUE la carte d'altitude, et met à jour les seules clés
qui en dépendent (cols, rows, elev_min, elev_max) dans un terrain.txt existant.
L'orthophoto, le point de départ et les fichiers annexes sont laissés intacts :
c'est ce qu'il faut pour affiner le relief d'une carte déjà en place sans
déplacer d'un mètre ce qui a été vérifié dessus.

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import os
import sys
import time

# Le paquet terrain/ est à côté de ce script : on s'assure que tools/ est dans le
# chemin d'import, quel que soit le répertoire d'appel.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from terrain import config
from terrain.ortho import fetch_ortho
from terrain.outputs import (write_exclusions, write_hapi, write_helipads, write_landmarks,
                             write_metadata)
from terrain.relief import fetch_heightmap, find_flat_start, write_heightmap
from terrain.zones import DEFAULT_ZONE


def maj_relief_seul(elev_min, elev_max):
    """Réécrit les seules clés de terrain.txt qui dépendent de la grille, en
       conservant tout le reste ligne par ligne (ortho, point de départ, cap,
       décalage d'origine, commentaires)."""
    chemin = os.path.join(config.OUT_DIR, "terrain.txt")
    remplace = {
        "cols": str(config.COLS),
        "rows": str(config.ROWS),
        "elev_min": f"{elev_min:.2f}",
        "elev_max": f"{elev_max:.2f}",
    }
    with open(chemin, encoding="utf-8") as f:
        lignes = f.readlines()
    sorties = []
    for ligne in lignes:
        cle = ligne.split(None, 1)[0] if ligne.strip() and not ligne.startswith("#") else None
        sorties.append(f"{cle} {remplace.pop(cle)}\n" if cle in remplace else ligne)
    if remplace:
        raise SystemExit(f"terrain.txt sans les clés {', '.join(remplace)} : "
                         "régénérer la carte entière plutôt qu'en relief seul.")
    with open(chemin, "w", encoding="utf-8") as f:
        f.writelines(sorties)
    print(f"[meta] {chemin} mis à jour (cols/rows/elev_min/elev_max)")


def main():
    # Zone choisie : premier argument non optionnel, sinon la zone par défaut.
    options = [a for a in sys.argv[1:] if a.startswith("--")]
    positionnels = [a for a in sys.argv[1:] if not a.startswith("--")]
    relief_seul = "--relief-seul" in options
    if set(options) - {"--relief-seul"}:
        raise SystemExit(f"option inconnue : {' '.join(set(options) - {'--relief-seul'})}")
    zone = positionnels[0] if positionnels else DEFAULT_ZONE
    config.select_zone(zone)

    if relief_seul:
        print(f"[zone] {zone} -> {config.OUT_DIR} (relief seul)")
        debut = time.time()
        grille = fetch_heightmap()
        elev_min, elev_max = write_heightmap(grille)
        maj_relief_seul(elev_min, elev_max)
        print(f"[ok] relief refait en {time.time() - debut:.0f} s")
        return

    print(f"[zone] {zone} -> {config.OUT_DIR}")
    os.makedirs(config.OUT_DIR, exist_ok=True)

    # Dimensions métriques de l'emprise (projection équirectangulaire locale,
    # très précise sur ~20 km). X = est, Z = sud dans le moteur.
    lat_center = math.radians((config.LAT_MIN + config.LAT_MAX) / 2.0)
    m_per_deg_lat = 111320.0
    m_per_deg_lon = 111320.0 * math.cos(lat_center)
    width_m = (config.LON_MAX - config.LON_MIN) * m_per_deg_lon
    height_m = (config.LAT_MAX - config.LAT_MIN) * m_per_deg_lat
    print(f"[zone] emprise ~ {width_m / 1000:.1f} km (E-O) x {height_m / 1000:.1f} km (N-S)")

    start = time.time()
    grid = fetch_heightmap()
    elev_min, elev_max = write_heightmap(grid)
    ortho_w = fetch_ortho(width_m / height_m)
    start_x, start_z = find_flat_start(grid, width_m, height_m)
    write_metadata(elev_min, elev_max, width_m, height_m, ortho_w, start_x, start_z)
    write_landmarks()
    write_helipads()
    write_exclusions()
    write_hapi()
    print(f"[ok] terminé en {time.time() - start:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
