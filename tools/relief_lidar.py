#!/usr/bin/env python3
"""
relief_lidar.py
Regénère le relief d'une carte à partir du LiDAR HD de l'IGN, au lieu du RGE
ALTI que fetch_terrain.py utilise. Même service, même format : seule la couche
change (voir tools/lidar/services.py).

Pourquoi. Le RGE ALTI servi à 1 m est un rééchantillonnage d'une pyramide plus
grossière ; à l'ombrage il est lisse et strié. Le MNT LiDAR porte du vrai détail
métrique. Sur une petite carte, où l'on peut se payer une grille fine, l'écart
est visible à l'oeil.

Par défaut l'outil écrit dans une carte NOUVELLE, copie de la source : la carte
d'origine n'est jamais touchée, et les deux se comparent en vol en changeant de
carte dans le menu.

Attention : le MNT est le sol NU. Les bâtiments n'y sont pas, et c'est ce qu'on
veut pour un relief (le moteur les extrude par ailleurs depuis la BD TOPO). Le
MNS, lui, les inclut : à réserver aux modèles d'objets, pas au terrain sur
lequel on se pose.

Données : IGN Géoplateforme (LiDAR HD), Licence Ouverte Etalab 2.0.
Dépendances : Python 3, numpy, Pillow.

Usage :
  python3 tools/relief_lidar.py dax-arene                    -> dax-arene-lidar
  python3 tools/relief_lidar.py dax-arene --grille 1024      grille plus fine
  python3 tools/relief_lidar.py dax-arene --sortie essai     autre nom de sortie
  python3 tools/relief_lidar.py dax-arene --sur-place        écrase la carte source

Auteur : O. Booklage
Licence : GPL v2
"""

import argparse
import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from terrain import config
from terrain.meta import read_meta, update_keys
from lidar.services import COUCHE_MNS, COUCHE_MNT, grille_altitudes

SURECHANTILLONNAGE = 4


def copier_carte(source, cible):
    """Copie une carte dans un nouveau dossier. Tous les fichiers annexes
       (orthophoto, lieux, hélipads, bâtiments, exclusions) restent valides :
       ils sont en coordonnées géographiques ou monde, que le relief ne change
       pas."""
    cible.mkdir(parents=True, exist_ok=True)
    for fichier in sorted(source.iterdir()):
        if fichier.is_file():
            shutil.copy2(fichier, cible / fichier.name)


def ecrire_heightmap(chemin, altitudes):
    """Écrit la grille en PNG 16 bits, normalisée entre 0 et son maximum, comme
       le fait terrain/relief.py. Renvoie l'altitude maximale."""
    propre = np.where(altitudes > config.NODATA, np.maximum(altitudes, 0.0), 0.0)
    maximum = float(propre.max())
    etendue = maximum if maximum > 0.0 else 1.0
    niveaux = np.round(propre / etendue * 65535.0).astype(np.uint16)
    Image.fromarray(niveaux, mode="I;16").save(chemin)
    return maximum


def main():
    analyseur = argparse.ArgumentParser(description=__doc__.split("\n")[2])
    analyseur.add_argument("carte", help="carte source (dossier sous assets/terrain/)")
    analyseur.add_argument("--sortie", default=None,
                           help="nom de la carte à écrire (défaut : <carte>-lidar)")
    analyseur.add_argument("--sur-place", action="store_true",
                           help="écrase le relief de la carte source, sans copie")
    analyseur.add_argument("--grille", type=int, default=None,
                           help="nombre de points par côté (défaut : celui de la carte)")
    analyseur.add_argument("--surech", type=int, default=SURECHANTILLONNAGE,
                           help="suréchantillonnage avant moyenne (défaut 4)")
    analyseur.add_argument("--surface", action="store_true",
                           help="prend le MNS (bâti et végétation compris) au lieu du sol nu")
    args = analyseur.parse_args()

    source = Path(config.TERRAIN_ROOT) / args.carte
    if not (source / "terrain.txt").exists():
        raise RuntimeError(f"carte inconnue : {source}")
    cible = source if args.sur_place else \
        Path(config.TERRAIN_ROOT) / (args.sortie or f"{args.carte}-lidar")

    meta = read_meta(source / "terrain.txt")
    bornes = (float(meta["lon_min"]), float(meta["lon_max"]),
              float(meta["lat_min"]), float(meta["lat_max"]))
    nx = nz = args.grille or int(meta["cols"])
    if args.grille is None:
        nx, nz = int(meta["cols"]), int(meta["rows"])

    largeur_m, hauteur_m = float(meta["width_m"]), float(meta["height_m"])
    print(f"[carte] {args.carte} -> {cible.name} : {largeur_m:.0f} x {hauteur_m:.0f} m, "
          f"grille {nx}x{nz}, maille {largeur_m / (nx - 1):.1f} m "
          f"(avant : {largeur_m / (int(meta['cols']) - 1):.1f} m)")

    if cible != source:
        copier_carte(source, cible)

    couche = COUCHE_MNS if args.surface else COUCHE_MNT
    altitudes = grille_altitudes(couche, bornes, nx, nz, max(1, args.surech))
    manquant = float(np.mean(altitudes <= config.NODATA)) * 100.0
    if manquant > 0.0:
        print(f"[relief] {manquant:.1f} % de l'emprise sans donnée LiDAR, ramenée au niveau 0")

    maximum = ecrire_heightmap(cible / "heightmap.png", altitudes)
    update_keys(cible / "terrain.txt",
                {"cols": nx, "rows": nz, "elev_max": f"{maximum:.2f}"})
    print(f"[relief] {cible / 'heightmap.png'} écrit, altitude 0 -> {maximum:.1f} m")
    print(f"[ok] carte {cible.name} prête ; la choisir dans le menu des cartes "
          f"ou par ARTOUSTE_TERRAIN={cible.name}")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
