#!/usr/bin/env python3
"""
observatoire.py
Fabrique le modèle 3D de l'observatoire du Pic du Midi de Bigorre à partir des
données publiques de l'IGN, et le pose dans la carte bigorre.

Ce fichier ne porte que ce qui est PROPRE À CE SITE : son emprise et ses
réglages. La fabrication elle-même vit dans lidar/piece.py, et sert aussi bien à
un aérodrome ou à un village (voir tools/piece_surface.py).

  forme    MNS issu du LiDAR HD, sol nu MNT pour distinguer le bâti du rocher
  texture  BD ORTHO à 0,20 m par pixel, même emprise

Données : IGN Géoplateforme (LiDAR HD, BD ORTHO), Licence Ouverte Etalab 2.0.

Usage :
  python3 tools/observatoire.py               fabrique le modèle
  python3 tools/observatoire.py --verifier    recontrôle le fichier écrit
  python3 tools/observatoire.py --pas 1.5     maillage plus léger

Sortie : assets/models/monuments/pic-du-midi/observatoire.{glb,jpg}

Auteur : O. Booklage
Licence : GPL v2
"""

import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lidar.carte import MARGE_FONDU
from lidar.piece import fabriquer, verifier
from lidar.services import ORTHO_M_PAR_PX

RACINE = Path(__file__).resolve().parent.parent

# Centre mesuré sur le masque MNS - MNT > 2 m : l'amas bâti se tient à 60 m à
# l'est et 26 m au nord de l'hélisurface déclarée (0,1411 / 42,9369).
CENTRE_LON, CENTRE_LAT = 0.14184, 42.93667
DEMI_X, DEMI_Z = 160.0, 130.0   # demi-boîte est-ouest et nord-sud, en mètres
PAS = 1.0                       # pas du maillage, en mètres


def main():
    analyseur = argparse.ArgumentParser(description=__doc__.split("\n")[2])
    analyseur.add_argument("--lon", type=float, default=CENTRE_LON)
    analyseur.add_argument("--lat", type=float, default=CENTRE_LAT)
    analyseur.add_argument("--demi-x", type=float, default=DEMI_X)
    analyseur.add_argument("--demi-z", type=float, default=DEMI_Z)
    analyseur.add_argument("--pas", type=float, default=PAS)
    analyseur.add_argument("--marge", type=float, default=MARGE_FONDU)
    analyseur.add_argument("--ortho-m-par-px", type=float, default=ORTHO_M_PAR_PX)
    analyseur.add_argument("--facade", type=Path, default=None,
                           help="photo de façade redressée, plaquée sur les murs")
    analyseur.add_argument("--verifier", action="store_true",
                           help="recontrôle le modèle déjà écrit, sans rien télécharger")
    args = analyseur.parse_args()
    args.carte = "bigorre"
    args.nom = "observatoire"
    args.titre = "Observatoire du Pic du Midi"
    args.coupoles = True   # le sommet en porte huit, que le MNS écrase
    args.sortie = RACINE / "assets" / "models" / "monuments" / "pic-du-midi"

    if args.verifier:
        verifier(args)
    else:
        debut = time.time()
        fabriquer(args)
        verifier(args)
        print(f"[ok] terminé en {time.time() - debut:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
