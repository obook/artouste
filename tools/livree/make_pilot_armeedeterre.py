#!/usr/bin/env python3
"""
make_pilot_armeedeterre.py
Génère la tenue armée de terre (ALAT) du pilote à partir de l'atlas d'origine
du modèle (general_pilot.png) : combinaison vert olive #4b4e35 (même teinte
que le fuselage, voir make_armeedeterre.py) et casque kaki, plus clair que la
combinaison. Voir pilot.py pour le principe (rectangles de l'atlas,
étirement de luminance). L'image d'origine n'est jamais modifiée : on écrit un
fichier séparé.

Usage : python3 tools/livree/make_pilot_armeedeterre.py [src] [dst]
Sortie par défaut :
  assets/models/Alouette-II/Models/Pilot/general_pilot-armeedeterre.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

from pilot import make_pilot_outfit

SRC_DEFAUT = "assets/models/Alouette-II/Models/Pilot/general_pilot.png"
DST_DEFAUT = "assets/models/Alouette-II/Models/Pilot/general_pilot-armeedeterre.png"

SHIRT_HEX = (0x4b, 0x4e, 0x35)
HELMET_HEX = (0x8a, 0x7f, 0x52)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAUT
    dst = sys.argv[2] if len(sys.argv) > 2 else DST_DEFAUT
    make_pilot_outfit(src, dst, SHIRT_HEX, HELMET_HEX)


if __name__ == "__main__":
    main()
