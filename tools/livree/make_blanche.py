#!/usr/bin/env python3
"""
make_blanche.py
Génère une livrée toute blanche à partir de l'atlas d'origine de l'Alouette II.
On repeint en blanc cassé #f0f0ec (RGB 240, 240, 236) uniquement les pixels
neutres (le "métal" gris) et l'accent orange de la poutre, en laissant intacts
les marquages saturés (cocardes tricolores). Voir retint.py pour le principe
(partagé avec make_gendarmerie.py, make_armeedeterre.py,
make_protectioncivile.py) : c'est le pendant blanc de make_gendarmerie.py, dont
seule la couleur cible change. Un blanc légèrement moins lumineux que le blanc
pur (#ffffff) évite de saturer trop de pixels neutres en blanc absolu, ce qui
aplatirait le modelé du fuselage et rendrait la livrée éblouissante en plein
soleil : en visant #f0f0ec, les hautes lumières restent claires mais l'ombrage
(lignes de tôle, recoins) reste lisible. L'image d'origine n'est jamais
modifiée : on écrit un fichier séparé.

Usage : python3 tools/livree/make_blanche.py [src] [dst]
Sortie par défaut : assets/models/Alouette-II/Models/texture-blanche.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

from retint import retint

SRC_DEFAUT = "assets/models/Alouette-II/Models/texture.png"
DST_DEFAUT = "assets/models/Alouette-II/Models/texture-blanche.png"
CIBLE = (0xf0, 0xf0, 0xec)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAUT
    dst = sys.argv[2] if len(sys.argv) > 2 else DST_DEFAUT
    retint(src, dst, CIBLE)


if __name__ == "__main__":
    main()
