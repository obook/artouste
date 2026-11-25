#!/usr/bin/env python3
"""
rose_compas.py
Dessine la rose du compas magnétique du poste 3dexport : une bande qui fait le
tour complet (360 deg sur toute la largeur), graduée tous les 5 deg, chiffrée
tous les 30 deg (N 3 6 E 12 15 S 21 24 W 30 33). Le moteur la fait défiler
derrière la fenêtre du compas selon le cap (LoadedHelicopterPlanche.cpp).

Comme sur un compas à bain d'huile, la rose se lit par la tranche : les caps
croissent vers la gauche. Le bord gauche de l'image est le nord, puis 355,
350... vers la droite.

Usage :
    python3 tools/modele/rose_compas.py \\
        assets/models/Alouette-II/Models/Interior/rose-compas.png

Auteur : O. Booklage
Date : novembre 2026
Licence : GPL v2
"""

import sys

from PIL import Image, ImageDraw, ImageFont

LARGEUR, HAUTEUR = 2048, 256
SURECH = 2
POLICE = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"
FOND = (18, 18, 20)
TRAIT = (235, 235, 228)
NOMS = {0: "N", 90: "E", 180: "S", 270: "W"}


def main():
    sortie = sys.argv[1]
    W, H = LARGEUR * SURECH, HAUTEUR * SURECH
    img = Image.new("RGB", (W, H), FOND)
    d = ImageDraw.Draw(img)
    try:
        f = ImageFont.truetype(POLICE, int(H * 0.42))
    except OSError:
        f = ImageFont.load_default()
    for cap in range(0, 360, 5):
        # Caps croissants vers la gauche ; on dessine aussi la copie décalée
        # d'un tour pour que les chiffres coupés au bord se raccordent.
        for tour in (0, W):
            x = (360 - cap) % 360 / 360.0 * W - tour
            for xx in (x, x + W):
                long = 0.30 if cap % 10 == 0 else 0.18
                d.line([(xx, 0), (xx, H * long)], fill=TRAIT, width=SURECH * (3 if cap % 30 == 0 else 2))
                if cap % 30 == 0:
                    texte = NOMS.get(cap, str(cap // 10))
                    d.text((xx, H * 0.66), texte, font=f, fill=TRAIT, anchor="mm")
    img.resize((LARGEUR, HAUTEUR), Image.LANCZOS).save(sortie, optimize=True)
    print("rose_compas : écrit", sortie)


if __name__ == "__main__":
    main()
