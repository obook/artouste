#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generer_detail_roche.py
Genere la texture de detail rocheuse du terrain : un bruit multi-octaves
PERIODIQUE (synthese spectrale par FFT, donc tuilable par construction),
anisotrope (strates) avec des micro-failles sombres. Sortie : PNG 512x512 en
niveaux de gris, assets/textures/detail-roche.png, echantillonne par
terrain.frag en coordonnees monde (tuile de 1,5 m).

Usage : python3 tools/detail/generer_detail_roche.py [graine]
Dependances : NumPy, Pillow.

Auteur : O. Booklage
Licence : GPL v2
"""

import os
import sys

import numpy as np
from PIL import Image

TAILLE = 512          # cote du PNG (pixels)
ANISOTROPIE = 2.2     # etirement des frequences : strates allongees
PENTE_SPECTRE = 1.6   # 1/f^pente : equilibre grandes formes / grain fin
LARGEUR_FAILLE = 0.08 # finesse des micro-failles (ecart-type du bruit)
PROFONDEUR_FAILLE = 0.35  # assombrissement au fond des failles


def bruit_periodique(rng, pente):
    """Bruit 2D periodique par synthese spectrale : bruit blanc filtre en
       1/f^pente dans le domaine de Fourier. L'IFFT est periodique par
       construction, donc la texture se tuile sans couture."""
    blanc = rng.standard_normal((TAILLE, TAILLE))
    f = np.fft.fftfreq(TAILLE)
    kx, ky = np.meshgrid(f, f)
    k = np.sqrt((kx * ANISOTROPIE) ** 2 + ky ** 2)
    k[0, 0] = 1.0  # evite la division par zero sur la composante continue
    spectre = np.fft.fft2(blanc) / (k ** pente)
    spectre[0, 0] = 0.0  # champ centre (pas de biais global)
    bruit = np.real(np.fft.ifft2(spectre))
    return (bruit - bruit.mean()) / bruit.std()


def main():
    graine = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    rng = np.random.default_rng(graine)

    relief = bruit_periodique(rng, PENTE_SPECTRE)
    # Micro-failles : lignes sombres la ou un second bruit passe par zero.
    failles = bruit_periodique(rng, 1.2)
    creux = np.exp(-((failles / LARGEUR_FAILLE) ** 2))

    image = relief * 0.5 + 0.5 - creux * PROFONDEUR_FAILLE
    image = (image - image.min()) / (image.max() - image.min())

    racine = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
    sortie = os.path.join(racine, "assets", "textures", "detail-roche.png")
    os.makedirs(os.path.dirname(sortie), exist_ok=True)
    Image.fromarray((image * 255.0).astype(np.uint8), "L").save(sortie)
    print(f"[detail] {sortie} ecrit ({TAILLE}x{TAILLE}, graine {graine})")


if __name__ == "__main__":
    main()
