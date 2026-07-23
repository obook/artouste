#!/usr/bin/env python3
"""
make_gendarmerie.py
Génère une livrée Gendarmerie nationale à partir de l'atlas d'origine de
l'Alouette II. On repeint en bleu gendarmerie #374f6b (RGB 55, 79, 107, un bleu
ardoise foncé et sourd) uniquement les pixels neutres (le "métal" gris) et
l'accent orange de la poutre, en laissant intacts les marquages saturés
(cocardes, orange). Voir retint.py pour le principe (partagé avec
make_armeedeterre.py, make_blanche.py, make_protectioncivile.py). L'image
d'origine n'est jamais modifiée : on écrit un fichier séparé.

Usage : python3 tools/livree/make_gendarmerie.py [src] [dst]
Sortie par défaut : assets/models/Alouette-II/Models/texture-gendarmerie.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

from retint import retint

SRC_DEFAUT = "assets/models/Alouette-II/Models/texture.png"
DST_DEFAUT = "assets/models/Alouette-II/Models/texture-gendarmerie.png"
CIBLE = (0x37, 0x4f, 0x6b)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAUT
    dst = sys.argv[2] if len(sys.argv) > 2 else DST_DEFAUT
    retint(src, dst, CIBLE)


if __name__ == "__main__":
    main()
