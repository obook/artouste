#!/usr/bin/env python3
"""
make_pilot_gendarmerie.py
Génère la tenue Gendarmerie du pilote à partir de l'atlas d'origine du modèle
(general_pilot.png) : combinaison bleu gendarmerie #374f6b (même teinte que le
fuselage, voir make_gendarmerie.py) et casque blanc. Voir pilot.py pour le
principe (rectangles de l'atlas, étirement de luminance). L'image d'origine
n'est jamais modifiée : on écrit un fichier séparé.

Usage : python3 tools/livree/make_pilot_gendarmerie.py [src] [dst]
Sortie par défaut :
  assets/models/Alouette-II/Models/Pilot/general_pilot-gendarmerie.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

from pilot import make_pilot_outfit

SRC_DEFAUT = "assets/models/Alouette-II/Models/Pilot/general_pilot.png"
DST_DEFAUT = "assets/models/Alouette-II/Models/Pilot/general_pilot-gendarmerie.png"

# Casque blanc (la visière fumée n'est pas modélisée séparément dans l'atlas).
SHIRT_HEX = (0x37, 0x4f, 0x6b)
HELMET_HEX = (0xff, 0xff, 0xff)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAUT
    dst = sys.argv[2] if len(sys.argv) > 2 else DST_DEFAUT
    make_pilot_outfit(src, dst, SHIRT_HEX, HELMET_HEX)


if __name__ == "__main__":
    main()
