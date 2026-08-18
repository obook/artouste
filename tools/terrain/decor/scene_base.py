#!/usr/bin/env python3
"""
Nom du fichier : scene_base.py
Description : Composition du décor de la base : pistes, tabliers, bâtiments et
              véhicules posés sur le plan.
Auteur : O. Booklage
Date : Août 2026
Licence : GPL v2
"""

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

from decor.formes_base import (ARBRE, AXE, BETON, BETON_CLAIR, BETON_USE,
                               BITUME, BITUME_USE, HELICO, HERBE, JOINT,
                               MARQUE, MARQUE_J, OMBRE, OMBRE_AZIMUT,
                               TOIT_TOLE2,
                               OMBRE_FACTEUR, TOIT_MAT, TOIT_PLAT, TOIT_TOLE,
                               VEHICULE, Plan, appareil, batiment, hangar,
                               surface_lisse, texture, voiture)

def dessiner(taille_px, centre_px, mpp, textures):
    p = Plan(taille_px, centre_px, mpp)
    herbe, beton, bitume = textures
    p.img.paste(herbe, (0, 0)); p.dr = ImageDraw.Draw(p.img)

    # --- aire de stationnement le long de la piste et sa voie de circulation
    p.dalle(-780, -560, 1500, 300, BETON, beton)
    p.dalle(-780, -290, 1500, 40, BITUME, bitume)
    if p.fin:
        for k in range(int(1500 / 25)):                   # joints de dalle
            p.ligne(-780 + k * 25, -560, -780 + k * 25, -260, 0.25, JOINT)
        for k in range(int(300 / 25)):
            p.ligne(-780, -560 + k * 25, 720, -560 + k * 25, 0.25, JOINT)
        p.ligne(-780, -270, 720, -270, 0.6, MARQUE_J)     # axe de la voie

    # --- plots de poser, numérotés, avec quelques appareils garés
    rng = np.random.default_rng(5)
    for k in range(14):
        u = -720 + k * 105
        for v in (-470, -370):
            p.disque(u, v, 16, BETON_USE)
            if p.fin:
                p.disque(u, v, 15, None, contour=MARQUE, epaisseur_m=0.6)
                p.ligne(u - 6, v, u + 6, v, 0.5, MARQUE)
            if rng.random() < 0.45:
                appareil(p, u, v, graine=k * 7 + (0 if v < -400 else 1))

    # --- hangars et leur desserte
    for k in range(6):
        hangar(p, -700 + k * 190, -200, 130, 70)
    p.dalle(-780, -110, 1500, 26, BITUME, bitume)
    if p.fin:
        p.ligne(-780, -97, 720, -97, 0.5, MARQUE)

    # --- quartier technique : bâtiments, rues, stationnements
    rng = np.random.default_rng(7)
    for rangee in range(4):
        for k in range(7):
            u = -640 + k * 165 + rng.uniform(-15, 15)
            v = 40 + rangee * 130 + rng.uniform(-10, 10)
            lu, lv = rng.uniform(55, 110), rng.uniform(28, 48)
            toit = (TOIT_MAT, TOIT_PLAT, TOIT_TOLE2)[(k + rangee) % 3]
            batiment(p, u, v, lu, lv, rng.uniform(7, 13), toit)
    for rangee in range(5):
        p.dalle(-700, 10 + rangee * 130, 1350, 14, BITUME, bitume)
        if p.fin:
            for k in range(int(1350 / 12)):               # ligne axiale discontinue
                p.ligne(-700 + k * 12, 17, -694 + k * 12, 17, 0.15, MARQUE)
    for k in range(6):
        p.dalle(-620 + k * 230, 10, 12, 540, BITUME, bitume)

    # --- parkings véhicules, remplis
    for k in range(3):
        u0 = -660 + k * 150
        p.dalle(u0, 570, 120, 60, BITUME, bitume)
        if p.fin:
            for i in range(int(120 / 5.2)):
                p.ligne(u0 + 2 + i * 5.2, 572, u0 + 2 + i * 5.2, 598, 0.15, MARQUE)
                if rng.random() < 0.6:
                    voiture(p, u0 + 3 + i * 5.2, 574, cap_u=False, graine=k * 40 + i)

    # --- route de ceinture et rideau d'arbres
    p.dalle(-820, -600, 20, 1260, BITUME, bitume)
    p.dalle(760, -600, 20, 1260, BITUME, bitume)
    rng = np.random.default_rng(21)
    for v in np.arange(-560, 640, 26):
        for u in (-860, 800):
            uu = u + rng.uniform(-12, 12); vv = v + rng.uniform(-10, 10)
            r = rng.uniform(5, 10)
            d = 9.0 * OMBRE_FACTEUR
            p.dro.ellipse([p.px(uu + d, vv)[0] - r / mpp, p.px(uu + d, vv)[1] - r / mpp,
                           p.px(uu + d, vv)[0] + r / mpp, p.px(uu + d, vv)[1] + r / mpp], fill=OMBRE)
            p.disque(uu, vv, r, ARBRE)

    # les ombres, adoucies, se posent sur l'ensemble
    o = np.asarray(p.ombres).astype(np.float32)
    o[..., 3] = ndimage.gaussian_filter(o[..., 3], max(0.6, 0.9 / mpp))
    p.img = Image.alpha_composite(p.img.convert("RGBA"),
                                  Image.fromarray(o.astype(np.uint8))).convert("RGB")
    return p.img
