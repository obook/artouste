#!/usr/bin/env python3
"""
usure.py
Peint la salissure de l'Alouette II dans une carte 2048 (1 = propre), à partir
des cartes de position et de normale cuites par cuire_geometrie.py. Tout se
décide en mètres, dans le repère du .ac (nez vers -X, Y vers le haut) :

  - suie : panache sombre derrière l'échappement de la turbine, qui file vers
    la queue le long de la poutre ;
  - coulures : traînées verticales sur les flancs, là où la pluie descend ;
  - crasse : le bas de la cellule est plus sale que le haut, et un grain
    irrégulier casse les aplats.

Les texels que le dépliage n'utilise pas restent à 1. retint.py multiplie la
livrée par cette carte si elle est posée à côté de la source (texture-usure.png),
après le repeint : la crasse s'applique à toutes les livrées de la même façon.

Usage : python3 tools/livree/usure.py prefixe_geo sortie.png
        (lit prefixe_geo-position.png et prefixe_geo-normale.png)

Auteur : O. Booklage
Date : novembre 2026
Licence : GPL v2
"""

import sys

import numpy as np
from PIL import Image, ImageFilter

PORTEE_M = 5.0  # même codage que cuire_geometrie.py : [-5, 5] m -> [0, 1]

ECHAPPEMENT = np.array([-1.25, 0.30, 0.0])  # sortie de la turbine, arrière du moteur
SUIE_LONGUEUR_M = 1.8   # le panache s'étire vers la queue (+X)
SUIE_RAYON_M = 0.35     # rayon à la sortie, il s'élargit en filant
SUIE_FORCE = 0.70       # 0 = rien, 1 = noir

COULURE_FORCE = 0.28    # assombrissement d'une traînée
CRASSE_HAUT_M = 0.2     # au-dessus, la tôle reste propre
CRASSE_BAS_M = -1.3     # en dessous, crasse maximale
CRASSE_FORCE = 0.22
GRAIN_FORCE = 0.07
GRAINE = 313


def bruit(p, frequence, graine=0.0):
    """Bruit de valeur 3D, interpolé entre les coins d'une grille de pas 1/frequence.

       Les coins sont tirés par hachage, sans tableau de grille : la cellule tient
       dans [-5, 5] m mais le hachage marche pour toute coordonnée."""
    q = p * frequence
    base = np.floor(q)
    f = q - base
    f = f * f * (3.0 - 2.0 * f)

    def coin(dx, dy, dz):
        c = base + np.array([dx, dy, dz])
        h = np.sin(c[..., 0] * 127.1 + c[..., 1] * 311.7 + c[..., 2] * 74.7 + graine)
        return (h * 43758.5453) % 1.0

    def lerp(a, b, t):
        return a + (b - a) * t

    x0 = lerp(lerp(coin(0, 0, 0), coin(1, 0, 0), f[..., 0]),
              lerp(coin(0, 1, 0), coin(1, 1, 0), f[..., 0]), f[..., 1])
    x1 = lerp(lerp(coin(0, 0, 1), coin(1, 0, 1), f[..., 0]),
              lerp(coin(0, 1, 1), coin(1, 1, 1), f[..., 0]), f[..., 1])
    return lerp(x0, x1, f[..., 2])


def suie(p):
    """Panache : une goutte allongée vers +X depuis l'échappement, qui s'élargit
       et s'estompe avec la distance. Rendue irrégulière par un bruit."""
    d = p - ECHAPPEMENT
    along = np.clip(d[..., 0] / SUIE_LONGUEUR_M, 0.0, 1.0)
    rayon = SUIE_RAYON_M * (1.0 + 1.5 * along)
    radial = np.hypot(d[..., 1], d[..., 2]) / rayon
    avant = np.clip(1.0 + d[..., 0] / 0.5, 0.0, 1.0)  # presque rien devant l'échappement
    forme = np.exp(-radial * radial) * (1.0 - along) ** 1.5 * avant
    return forme * (0.6 + 0.8 * bruit(p, 6.0, 1.0))


def coulures(p, n, colonnes):
    """Traînées verticales sur les surfaces presque verticales. Le filet fin
       est tiré en colonnes de l'atlas (l'appareil y est dessiné à plat, la
       verticale de l'image suit celle de la tôle) : la position cuite, à 4 cm
       près, serait trop grossière pour un filet. Un bruit large en (x, z) les
       regroupe en paquets, et la crasse se renforce en descendant."""
    vertical = np.clip(1.0 - np.abs(n[..., 1]) / 0.7, 0.0, 1.0)
    plat = p.copy()
    plat[..., 1] = 0.0
    paquets = bruit(plat, 4.0, 3.0)
    filets = np.clip((colonnes * paquets - 0.22) / 0.25, 0.0, 1.0)
    descente = np.clip((CRASSE_HAUT_M - p[..., 1]) / (CRASSE_HAUT_M - CRASSE_BAS_M), 0.0, 1.0)
    return filets * vertical * (0.3 + 0.7 * descente)


def crasse(p):
    """Gradient de hauteur et grain irrégulier."""
    hauteur = np.clip((CRASSE_HAUT_M - p[..., 1]) / (CRASSE_HAUT_M - CRASSE_BAS_M), 0.0, 1.0)
    grain = bruit(p, 2.5, 4.0) * 0.6 + bruit(p, 9.0, 5.0) * 0.4
    return hauteur * (0.5 + 0.5 * grain), grain


def main():
    if len(sys.argv) < 3:
        raise SystemExit("Usage : usure.py prefixe_geo sortie.png")
    prefixe, sortie = sys.argv[1], sys.argv[2]
    pos = np.asarray(Image.open(prefixe + "-position.png").convert("RGB"), dtype=np.float64)
    nor = np.asarray(Image.open(prefixe + "-normale.png").convert("RGB"), dtype=np.float64)
    p = pos / 255.0 * (2.0 * PORTEE_M) - PORTEE_M
    n = nor / 255.0 * 2.0 - 1.0
    # Texel cuit : sa normale est unitaire. Un texel vide est noir, soit (-1,-1,-1).
    utilise = np.abs(np.linalg.norm(n, axis=-1) - 1.0) < 0.2

    niveau, grain = crasse(p)
    propre = 1.0 - CRASSE_FORCE * niveau
    propre *= 1.0 - GRAIN_FORCE * (grain - 0.5)
    # Filets d'un ou deux texels de large, constants le long des colonnes.
    alea = np.random.default_rng(GRAINE).uniform(0.0, 1.0, p.shape[1])
    colonnes = np.repeat(np.asarray(Image.fromarray((alea * 255).astype(np.uint8)[None, :])
                                    .filter(ImageFilter.GaussianBlur(0.7)), dtype=np.float64) / 255.0,
                         p.shape[0], axis=0)
    propre *= 1.0 - COULURE_FORCE * coulures(p, n, colonnes)
    propre *= 1.0 - SUIE_FORCE * np.clip(suie(p), 0.0, 1.0)
    propre = np.where(utilise, np.clip(propre, 0.0, 1.0), 1.0)

    im = Image.fromarray((propre * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.8))
    im.save(sortie)
    print(f"usure écrite -> {sortie}  (texels utilisés {utilise.mean() * 100:.0f}%, "
          f"assombrissement moyen {(1.0 - propre[utilise].mean()) * 100:.1f}%, "
          f"suie sur {(suie(p)[utilise] > 0.2).mean() * 100:.1f}%)")


if __name__ == "__main__":
    main()
