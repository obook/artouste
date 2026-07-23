#!/usr/bin/env python3
"""
make_pilot_protectioncivile.py
Génère la tenue Protection civile (sécurité civile) du pilote à partir de
l'atlas d'origine du modèle (general_pilot.png) : combinaison orange vif
#e8600a (la vraie tenue de la sécurité civile, pas la couleur rouge de
l'appareil, voir make_protectioncivile.py) et casque blanc. Voir pilot.py
pour le principe (rectangles de l'atlas, étirement de luminance). L'image
d'origine n'est jamais modifiée : on écrit un fichier séparé.

Usage : python3 tools/livree/make_pilot_protectioncivile.py [src] [dst]
Sortie par défaut :
  assets/models/Alouette-II/Models/Pilot/general_pilot-protectioncivile.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

from pilot import make_pilot_outfit

SRC_DEFAUT = "assets/models/Alouette-II/Models/Pilot/general_pilot.png"
DST_DEFAUT = "assets/models/Alouette-II/Models/Pilot/general_pilot-protectioncivile.png"

SHIRT_HEX = (0xe8, 0x60, 0x0a)
HELMET_HEX = (0xff, 0xff, 0xff)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAUT
    dst = sys.argv[2] if len(sys.argv) > 2 else DST_DEFAUT
    make_pilot_outfit(src, dst, SHIRT_HEX, HELMET_HEX)


if __name__ == "__main__":
    main()
