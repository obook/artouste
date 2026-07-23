#!/usr/bin/env python3
"""
make_protectioncivile.py
Génère une livrée Protection civile (Sécurité civile) à partir de l'atlas
d'origine de l'Alouette II. On repeint en rouge Protection civile officiel
#611716 (RGB 97, 23, 22, un rouge sombre et saturé) uniquement les pixels
neutres (le "métal" gris) et l'accent orange de la poutre, en laissant intacts
les marquages saturés (cocardes tricolores). Voir retint.py pour le principe
(partagé avec make_gendarmerie.py, make_armeedeterre.py, make_blanche.py) :
c'est le pendant rouge de make_gendarmerie.py, dont seule la couleur cible
change -- avec en plus un gain d'ombrage plafonné à (0.45, 1.30) : sans borne
haute, les reflets brillants du métal saturent le canal rouge à 1.0 pendant que
vert et bleu continuent de monter, ce qui délave le rouge vers le rose ; le
plancher à 0.45 garde les creux d'un rouge sombre plutôt que noir. L'image
d'origine n'est jamais modifiée : on écrit un fichier séparé.

Usage : python3 tools/livree/make_protectioncivile.py [src] [dst]
Sortie par défaut : assets/models/Alouette-II/Models/texture-protectioncivile.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

from retint import retint

SRC_DEFAUT = "assets/models/Alouette-II/Models/texture.png"
DST_DEFAUT = "assets/models/Alouette-II/Models/texture-protectioncivile.png"
CIBLE = (0x61, 0x17, 0x16)
GAIN_CLIP = (0.45, 1.30)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAUT
    dst = sys.argv[2] if len(sys.argv) > 2 else DST_DEFAUT
    retint(src, dst, CIBLE, gain_clip=GAIN_CLIP)


if __name__ == "__main__":
    main()
