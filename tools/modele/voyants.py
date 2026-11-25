#!/usr/bin/env python3
"""
voyants.py
Dessine la texture des coupoles des voyants de la console du poste 3dexport :
six bandes de 16 px de haut (assez pour que les mipmaps ne les mélangent
pas), une par couleur et par état (rouge, vert, ambre, allumés puis éteints). Le long de chaque bande, le dégradé va du sommet de la
coupole (à gauche) à son bord (à droite) : cœur presque blanc allumé, verre
sombre éteint. Le moteur choisit la bande par la coordonnée v
(LoadedHelicopterPlanche.cpp, même ordre).

Usage :
    python3 tools/modele/voyants.py \\
        assets/models/Alouette-II/Models/Interior/voyants.png

Auteur : O. Booklage
Date : novembre 2026
Licence : GPL v2
"""

import sys

from PIL import Image

LARGEUR, BANDE = 64, 16
COULEURS = [(255, 40, 25), (40, 255, 70), (255, 170, 20)]  # rouge, vert, ambre


def melange(a, b, t):
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


def allume(c, t):
    """Sommet presque blanc, puis la couleur, bord un peu plus sombre."""
    if t < 0.5:
        return melange(melange((255, 255, 255), c, 0.45), c, t / 0.5)
    return melange(c, tuple(int(x * 0.6) for x in c), (t - 0.5) / 0.5)


def eteint(c, t):
    """Verre coloré sombre, un peu plus clair au sommet."""
    return melange(tuple(int(x * 0.42) for x in c), tuple(int(x * 0.16) for x in c), t)


def main():
    sortie = sys.argv[1]
    img = Image.new("RGB", (LARGEUR, BANDE * 6))
    for rang, (fonction, couleur) in enumerate([(allume, c) for c in COULEURS] +
                                               [(eteint, c) for c in COULEURS]):
        for x in range(LARGEUR):
            pixel = fonction(couleur, x / (LARGEUR - 1))
            for y in range(BANDE):
                img.putpixel((x, rang * BANDE + y), pixel)
    img.save(sortie, optimize=True)
    print("voyants : écrit", sortie)


if __name__ == "__main__":
    main()
