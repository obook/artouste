#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generer_detail_roche.py
Génère la texture de détail rocheuse du terrain : un bruit multi-octaves
PÉRIODIQUE (synthèse spectrale par FFT, donc tuilable par construction),
anisotrope (strates) avec des micro-failles sombres. Sortie : PNG 512x512 en
niveaux de gris, assets/textures/detail-roche.png, échantillonné par
terrain.frag en coordonnées monde (tuile de 1,5 m).

Usage : python3 tools/detail/generer_detail_roche.py [graine]
Dépendances : NumPy, Pillow.

Auteur : O. Booklage
Licence : GPL v2
"""

import os
import sys

import numpy as np
from PIL import Image

TAILLE = 512          # côté du PNG (pixels)
ANISOTROPIE = 1.5     # étirement des fréquences : strates allongées
PENTE_SPECTRE = 1.6   # 1/f^pente : équilibre grandes formes / grain fin
LARGEUR_FAILLE = 0.08 # finesse des micro-failles (écart-type du bruit)
PROFONDEUR_FAILLE = 0.35  # assombrissement au fond des failles


def bruit_periodique(rng, pente):
    """Bruit 2D périodique par synthèse spectrale : bruit blanc filtré en
       1/f^pente dans le domaine de Fourier. L'IFFT est périodique par
       construction, donc la texture se tuile sans couture."""
    blanc = rng.standard_normal((TAILLE, TAILLE))
    f = np.fft.fftfreq(TAILLE)
    kx, ky = np.meshgrid(f, f)
    k = np.sqrt((kx * ANISOTROPIE) ** 2 + ky ** 2)
    k[0, 0] = 1.0  # évite la division par zéro sur la composante continue
    spectre = np.fft.fft2(blanc) / (k ** pente)
    spectre[0, 0] = 0.0  # champ centré (pas de biais global)
    bruit = np.real(np.fft.ifft2(spectre))
    return (bruit - bruit.mean()) / bruit.std()


def main():
    graine = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    rng = np.random.default_rng(graine)

    relief = bruit_periodique(rng, PENTE_SPECTRE)
    # Micro-failles : lignes sombres là où un second bruit passe par zéro.
    failles = bruit_periodique(rng, 1.2)
    creux = np.exp(-((failles / LARGEUR_FAILLE) ** 2))

    image = relief * 0.5 + 0.5 - creux * PROFONDEUR_FAILLE
    image = (image - image.min()) / (image.max() - image.min())

    racine = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
    sortie = os.path.join(racine, "assets", "textures", "detail-roche.png")
    os.makedirs(os.path.dirname(sortie), exist_ok=True)
    Image.fromarray((image * 255.0).astype(np.uint8), "L").save(sortie)
    print(f"[detail] {sortie} écrit ({TAILLE}x{TAILLE}, graine {graine})")


if __name__ == "__main__":
    main()
