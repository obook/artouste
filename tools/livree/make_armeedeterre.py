#!/usr/bin/env python3
"""
make_armeedeterre.py
Génère une livrée armée de terre (ALAT, Aviation légère de l'armée de terre) à
partir de l'atlas d'origine de l'Alouette II. On repeint en vert armée (olive
drab, proche RAL 6014) #4b4e35 (RGB 75, 78, 53) uniquement les pixels neutres
(le "métal" gris) et l'accent orange de la poutre, en laissant intacts les
marquages saturés (cocardes tricolores). Voir retint.py pour le principe
(partagé avec make_gendarmerie.py, make_blanche.py,
make_protectioncivile.py) : c'est le pendant olive de make_gendarmerie.py, dont
seule la couleur cible change. L'image d'origine n'est jamais modifiée : on
écrit un fichier séparé.

Usage : python3 tools/livree/make_armeedeterre.py [src] [dst]
Sortie par défaut : assets/models/Alouette-II/Models/texture-armeedeterre.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

from retint import retint

SRC_DEFAUT = "assets/models/Alouette-II/Models/texture-fond.png"
DST_DEFAUT = "assets/models/Alouette-II/Models/texture-armeedeterre.png"
CIBLE = (0x4b, 0x4e, 0x35)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAUT
    dst = sys.argv[2] if len(sys.argv) > 2 else DST_DEFAUT
    retint(src, dst, CIBLE)


if __name__ == "__main__":
    main()
