#!/usr/bin/env python3
"""
decor_base_pau.py
Décor IMAGINAIRE de la base d'hélicoptères de Pau-Uzein, dessiné à n'importe
quelle finesse (0,25 m par pixel pour les tuiles de détail, 1,80 m pour
l'orthophoto d'ensemble).

Rien ici ne prétend représenter les installations réelles, dont l'imagerie
officielle est floutée à la source : c'est un décor plausible pour un
simulateur, bâti sur le vocabulaire d'une base d'hélicoptères ordinaire.

Tout est dessiné en MÈTRES dans le repère de la piste (axe 125 degrés), jamais
en pixels : le même plan sert donc aux deux finesses, et le détail fin
(marquages, nervures de toiture, véhicules, appareils au parking) n'apparaît
que lorsque l'échelle le permet. Agrandir un dessin fait pour 1,80 m ne
donnerait que du flou.

Usage : decor_base_pau.py <sortie.png> <largeur_px> <hauteur_px> <mpp>
                          <lon_coin_no> <lat_coin_no>
"""
import math
import os
from pathlib import Path
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

# Morceaux d'orthophoto servant de textures : versionnés à côté de ce script,
# comme le masque de la zone militaire. ARTOUSTE_DECOR_TEXTURES permet d'en
# essayer d'autres sans toucher au dépôt.
TEXTURES_DIR = os.environ.get("ARTOUSTE_DECOR_TEXTURES",
                              str(Path(__file__).resolve().parent))

from decor.formes_base import (AXE, BETON, BITUME, surface_lisse, texture)
from decor.scene_base import dessiner


def main():
    sortie, W, H, mpp, lonNO, latNO = (sys.argv[1], int(sys.argv[2]), int(sys.argv[3]),
                                       float(sys.argv[4]), float(sys.argv[5]), float(sys.argv[6]))
    Image.MAX_IMAGE_PIXELS = None

    # Centre du décor : le centroïde du contour tracé à la main, en lon/lat.
    LON0, DLON = -0.4519550855, 0.0000222322
    LAT0, DLAT = 43.3943211600, 0.0000161700
    masque = Path(__file__).resolve().parent / "masque-zone-militaire-pau.png"
    m = np.asarray(Image.open(masque).convert("L")) > 128
    ys, xs = np.nonzero(m)
    lonC, latC = LON0 + xs.mean() * DLON, LAT0 - ys.mean() * DLAT
    mx = 111320 * math.cos(math.radians(latC))
    centre = ((lonC - lonNO) * mx / mpp, (latNO - latC) * 111320 / mpp)

    # Textures : de VRAIS morceaux d'orthophoto, pris à 25 cm sur les parties
    # nettes de la même plateforme (herbe au nord de la piste, béton de l'aire
    # civile, bitume d'une voie de circulation), puis ramenés à la finesse
    # demandée. Agrandir la photo d'ensemble à 1,80 m donnait des taches floues
    # aux coutures visibles : à 25 cm il faut de la donnée à 25 cm.
    def morceau(nom, cote_m=128.0, reference=None):
        """Un morceau de photo réelle, ramené à la finesse voulue. Les herbes
        prélevées à des endroits différents n'ont pas le même ton : on les
        recale sur la première, sans quoi le pavage dessine un damier."""
        chemin = Path(TEXTURES_DIR) / ("tex_%s.png" % nom)
        if not chemin.exists():
            raise SystemExit(
                "Texture absente : %s\n"
                "Les trois morceaux d'herbe sont normalement versionnés à côté de\n"
                "ce script. Les refaire : python3 -m decor.fetch_textures_pau\n"
                "Ou indiquer un autre dossier par ARTOUSTE_DECOR_TEXTURES." % chemin)
        im = Image.open(chemin).convert("RGB")   # 512 px = 128 m
        n = max(8, int(round(cote_m / mpp)))
        im = im.resize((n, n), Image.LANCZOS)
        if reference is not None:
            a = np.asarray(im, dtype=np.float32)
            b = np.asarray(reference, dtype=np.float32)
            for c in range(3):
                a[..., c] += b[..., c].mean() - a[..., c].mean()
            im = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
        return im

    herbe0 = morceau("herbe")
    textures = (texture([herbe0, morceau("herbe2", reference=herbe0),
                         morceau("herbe3", reference=herbe0)], (W, H)),
                surface_lisse(BETON, (W, H), 4.0, 3.0, mpp, 11),
                surface_lisse(BITUME, (W, H), 3.5, 2.5, mpp, 12))
    dessiner((W, H), centre, mpp, textures).save(sortie)
    print("décor écrit : %s (%d x %d, %.2f m/px)" % (sortie, W, H, mpp))


if __name__ == "__main__":
    main()
