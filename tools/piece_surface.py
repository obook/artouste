#!/usr/bin/env python3
"""
piece_surface.py
Fabrique une PIÈCE DE SURFACE : un morceau de terrain réel, relevé au laser,
posé sur une carte du simulateur comme un monument.

C'est l'outil général derrière tools/observatoire.py, pour tout autre sujet :
un aérodrome et ses hangars, un village, une station, une zone de poser. Partout
où l'on descend assez bas pour que la maille de la carte (14 à 18 m) et ses
bâtiments extrudés à toit plat ne suffisent plus.

Ce que la pièce apporte : la forme réelle des toits, des arbres, des talus et
des terrasses, texturée par l'orthophoto à 0,20 m/px, pour un seul appel de
dessin (0,19 ms mesuré sur une pièce de 320 x 260 m).

Ce qu'elle coûte : elle ne remplace pas le relief de la carte, elle se pose
dessus. Il faut donc effacer sous elle ce que le moteur ajoutait, sinon tout
compte double :

  - les bâtiments extrudés, par le rayon de dégagement de la ligne monuments.txt
    (dernier champ avant le fichier), que l'outil calcule et affiche ;
  - les arbres, par une ligne dans le exclusions.txt de la carte, que l'outil
    affiche aussi (voir tools/fetch_forest.py).

Données : IGN Géoplateforme (LiDAR HD, BD ORTHO), Licence Ouverte Etalab 2.0.

Usage :
  python3 tools/piece_surface.py --carte dax --nom hangars-dax \\
          --lon -1.0685 --lat 43.6905 --demi-x 220 --demi-z 160 \\
          --titre "Hangars de Dax-Seyresse"

  python3 tools/piece_surface.py --carte dax --nom hangars-dax --verifier

Auteur : O. Booklage
Licence : GPL v2
"""

import argparse
import math
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lidar.carte import MARGE_FONDU, M_PAR_DEG_LAT
from lidar.piece import fabriquer, verifier
from lidar.services import ORTHO_M_PAR_PX

RACINE = Path(__file__).resolve().parent.parent


def main():
    analyseur = argparse.ArgumentParser(description=__doc__.split("\n")[2])
    analyseur.add_argument("--carte", required=True, help="carte qui accueille la pièce")
    analyseur.add_argument("--nom", required=True,
                           help="nom du modèle et de son dossier sous assets/models/monuments/")
    analyseur.add_argument("--lon", type=float, required=True)
    analyseur.add_argument("--lat", type=float, required=True)
    analyseur.add_argument("--titre", default=None,
                           help="libellé affiché en jeu (défaut : le nom)")
    analyseur.add_argument("--demi-x", type=float, default=200.0,
                           help="demi-largeur est-ouest en mètres (défaut 200)")
    analyseur.add_argument("--demi-z", type=float, default=150.0,
                           help="demi-largeur nord-sud en mètres (défaut 150)")
    analyseur.add_argument("--pas", type=float, default=1.0,
                           help="pas du maillage en mètres (défaut 1 ; 1,5 allège de moitié)")
    analyseur.add_argument("--marge", type=float, default=MARGE_FONDU)
    analyseur.add_argument("--ortho-m-par-px", type=float, default=ORTHO_M_PAR_PX)
    analyseur.add_argument("--coupoles", action="store_true",
                           help="repère et rebâtit les coupoles (observatoires, silos)")
    analyseur.add_argument("--facade", type=Path, default=None)
    analyseur.add_argument("--verifier", action="store_true")
    args = analyseur.parse_args()
    args.titre = args.titre or args.nom
    args.sortie = RACINE / "assets" / "models" / "monuments" / args.nom

    if args.verifier:
        verifier(args)
        return

    debut = time.time()
    fabriquer(args)
    verifier(args)

    # Rayon à dégager : le cercle qui contient la pièce, pour qu'aucun bâtiment
    # extrudé ni aucun arbre ne pousse dessous.
    rayon = math.hypot(args.demi_x, args.demi_z)
    print(f"\n[exclusions.txt] ligne à ajouter dans assets/terrain/{args.carte}/, "
          f"pour que les arbres ne poussent pas dans la pièce :\n")
    print(f"{args.lon:.5f} {args.lat:.5f} {rayon:.0f}\n")
    print(f"[ok] terminé en {time.time() - debut:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
