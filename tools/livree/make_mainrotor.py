#!/usr/bin/env python3
"""
make_mainrotor.py
Uniformise la couleur du rotor principal de l'Alouette II en gris foncé.

Sur les photos, la tête rotor (moyeu) est de la même couleur que les pales :
un gris foncé uni. Dans la texture d'origine, le centre du moyeu était gris
clair métallique (~130) alors que les pales sont gris moyen (~70), d'où un
rotor bicolore peu fidèle.

L'outil repeint donc en gris foncé uni tous les pixels gris (faible saturation)
de mainrotor.png, et conserve le jaune (embout des pales, repères du moyeu) ainsi
que la transparence. Les couleurs posées sont absolues : le script est
ré-exécutable sans dérive (un second passage redonne le même résultat).

Contrairement à make_tailrotor.py, pas besoin de Blender : mainrotor.png ne sert
qu'au rotor principal (moyeu et pales ; les plans flous et le disque sont écartés
au runtime), donc un repeint direct en espace texture suffit.

Usage : python3 tools/livree/make_mainrotor.py

Auteur : O. Booklage
Licence : GPL v2
"""

import os

import numpy as np
from PIL import Image

RACINE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MR = os.path.join(RACINE, "assets", "models", "Alouette-II", "Models", "Externals", "MainRotor")
SRC_PNG = os.path.join(MR, "mainrotor.png")

# Gris foncé uni visé pour tout le corps du rotor (moyeu + pales). Valeur absolue,
# en 0..255. Un aplat uni reste fidèle au rendu voulu (rotor d'une seule couleur) et
# se compresse bien en PNG, contrairement à un grain par pixel qui gonflait le fichier.
GRIS = np.array([38, 38, 41], dtype=np.uint8)


def masque_jaune(rgb):
    """Renvoie le masque booléen des pixels jaunes à conserver (embout de pale,
       repères du moyeu) : rouge et vert élevés, bleu nettement plus faible."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    return (r > 110) & (g > 90) & (b < 95) & (r.astype(np.int32) - b.astype(np.int32) > 45)


def main():
    img = Image.open(SRC_PNG).convert("RGBA")
    px = np.asarray(img, dtype=np.uint8).copy()
    rgb = px[..., :3]
    alpha = px[..., 3]

    # On ne repeint que les pixels gris (faible saturation) et visibles : on
    # épargne le jaune et les zones totalement transparentes.
    maxc = rgb.max(axis=2).astype(np.int32)
    minc = rgb.min(axis=2).astype(np.int32)
    sature = (maxc - minc) > 35
    a_repeindre = (~masque_jaune(rgb)) & (~sature) & (alpha > 0)

    nb = int(a_repeindre.sum())
    px[..., :3] = np.where(a_repeindre[..., None], GRIS[None, None, :], rgb)

    # optimize=True : PNG bien compressé (l'aplat uni se réduit fortement).
    Image.fromarray(px, "RGBA").save(SRC_PNG, optimize=True)
    print("[mainrotor] pixels repeints en gris foncé :", nb)
    print("[mainrotor] écrit", SRC_PNG)


if __name__ == "__main__":
    main()
