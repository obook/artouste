#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_trees_atlas.py
Génère un atlas de sprites d'arbres pour la végétation en billboards.
Une seule image RGBA à fond transparent, découpée en N cellules carrées côte à
côte (une espèce par cellule), plaquée sur des billboards EN CROIX (deux quads
perpendiculaires) face au relief. Un arbre est centré dans sa cellule, base en
bas, avec des marges transparentes (pour éviter que le filtrage de texture ne
fasse baver une cellule sur sa voisine).

Trois espèces, choisies dans le moteur selon l'altitude et un tirage aléatoire :
  0 - conifère sombre (sapin), élancé ;
  1 - feuillu arrondi (hêtre / chêne), couronne en boule ;
  2 - mélèze clair, élancé et étroit (étage supérieur).

Réalisme visé (inspiré de FlightGear) : silhouette irrégulière, ombrage interne
(feuillage plus sombre à la base et au coeur, plus clair et lumineux vers le haut
et le côté soleil), et bords doux (léger flou) qui, combinés à l'alpha-to-coverage
du rendu, donnent un feuillage découpé plutôt qu'un aplat aux bords nets.

Assets PROCÉDURAUX (aucune donnée externe, aucune question de licence).
Remplaçables par les atlas FlightGear (Textures/Trees, GPL v2).

Usage : python3 tools/vegetation/make_trees_atlas.py
Sortie : assets/vegetation/trees_atlas.png (768x256, RGBA, 3 cellules de 256)

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

CELL = 256           # côté d'une cellule (px), taille finale
SS = 4               # supersampling (rendu en grand puis réduit)
W = CELL * SS
H = CELL * SS


def lerp(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def blob(d, cx, cy, r, color, alpha=255):
    """Disque plein (une touffe de feuillage)."""
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=color + (alpha,))


def shade(base, dark, light, t_vert, side):
    """Teinte d'une touffe : plus sombre en bas (t_vert 0) et au coeur, plus claire
    en haut et du côté soleil (side > 0)."""
    c = lerp(dark, light, 0.25 + 0.65 * t_vert + 0.15 * side)
    return c


def conifer(dark, light, trunk, seed):
    """Sapin : étages de branches en dents de scie, élancé, ombré du bas vers le haut."""
    rnd = random.Random(seed)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = W // 2
    base_y = int(H * 0.93)
    top_y = int(H * 0.07)
    # Tronc
    d.rectangle([cx - int(0.02 * W), int(H * 0.82), cx + int(0.02 * W), base_y],
                fill=trunk + (255,))
    tiers = 9
    foliage_bottom = int(H * 0.88)
    for k in range(tiers):
        t = k / (tiers - 1)                       # 0 bas -> 1 haut
        yb = int(foliage_bottom + (top_y - foliage_bottom) * (t * 0.86))
        half = (0.40 * W) * (1.0 - 0.72 * t)
        col = shade(dark, dark, light, t, 0.0)
        # touffes le long de la branche (dents de scie), plus claires vers la pointe
        n = max(3, int(10 * (1.0 - 0.6 * t)))
        for i in range(n):
            f = i / (n - 1) - 0.5                 # -0.5 .. 0.5 (gauche->droite)
            bx = cx + f * 2.0 * half + rnd.uniform(-0.02, 0.02) * W
            by = yb + rnd.uniform(-0.01, 0.03) * H
            r = (0.10 * W) * (1.0 - 0.5 * t) * rnd.uniform(0.8, 1.15)
            side = -f                              # soleil venant de la gauche
            blob(d, bx, by, r, shade(dark, dark, light, t, side))
        # pointe lumineuse au sommet du tier
        blob(d, cx, yb - half * 0.15, (0.06 * W) * (1.0 - 0.4 * t),
             lerp(col, light, 0.5))
    grain(img, top_y, foliage_bottom, rnd)
    return img


def broadleaf(dark, light, trunk, seed):
    """Feuillu : tronc court et couronne en amas de touffes, ombrée en boule."""
    rnd = random.Random(seed)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = W // 2
    base_y = int(H * 0.93)
    d.rectangle([cx - int(0.028 * W), int(H * 0.58), cx + int(0.028 * W), base_y],
                fill=trunk + (255,))
    crown_cy = int(H * 0.38)
    crown_r = 0.32 * W
    # couronne : touffes réparties dans un disque, ombrées (bas/coeur sombre, haut clair)
    for _ in range(60):
        a = rnd.uniform(0, 2 * math.pi)
        rr = crown_r * math.sqrt(rnd.uniform(0, 1)) * 0.85
        bx = cx + math.cos(a) * rr
        by = crown_cy - math.sin(a) * rr * 0.95
        t_vert = 0.5 - (by - crown_cy) / (2 * crown_r)   # haut -> 1
        side = math.cos(a)                                # soleil gauche
        r = crown_r * rnd.uniform(0.18, 0.30)
        blob(d, bx, by, r, shade(dark, dark, light, t_vert, side))
    grain(img, int(H * 0.06), int(H * 0.70), rnd)
    return img


def larch(dark, light, trunk, seed):
    """Mélèze : conifère clair, plus étroit et plus haut (étage supérieur)."""
    rnd = random.Random(seed)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = W // 2
    base_y = int(H * 0.94)
    top_y = int(H * 0.05)
    d.rectangle([cx - int(0.016 * W), int(H * 0.84), cx + int(0.016 * W), base_y],
                fill=trunk + (255,))
    tiers = 10
    foliage_bottom = int(H * 0.90)
    for k in range(tiers):
        t = k / (tiers - 1)
        yb = int(foliage_bottom + (top_y - foliage_bottom) * (t * 0.88))
        half = (0.24 * W) * (1.0 - 0.7 * t)
        n = max(2, int(7 * (1.0 - 0.5 * t)))
        for i in range(n):
            f = i / (n - 1) - 0.5 if n > 1 else 0.0
            bx = cx + f * 2.0 * half + rnd.uniform(-0.015, 0.015) * W
            by = yb + rnd.uniform(-0.01, 0.02) * H
            r = (0.07 * W) * (1.0 - 0.4 * t) * rnd.uniform(0.8, 1.1)
            blob(d, bx, by, r, shade(dark, dark, light, t, -f))
    grain(img, top_y, foliage_bottom, rnd)
    return img


def grain(img, top_y, bottom_y, rnd, n=9000, amp=0.16):
    """Petites touches plus claires / sombres sur le feuillage opaque (texture)."""
    px = img.load()
    w, h = img.size
    for _ in range(n):
        x = rnd.randint(0, w - 1)
        y = rnd.randint(top_y, bottom_y)
        r, g, b, a = px[x, y]
        if a < 180:
            continue
        s = rnd.uniform(-amp, amp)
        px[x, y] = (max(0, min(255, int(r * (1 + s)))),
                    max(0, min(255, int(g * (1 + s)))),
                    max(0, min(255, int(b * (1 + s)))), a)


def finish(img):
    """Bords doux (léger flou, bon pour l'alpha-to-coverage) puis réduction."""
    img = img.filter(ImageFilter.GaussianBlur(SS * 0.6))
    return img.resize((CELL, CELL), Image.LANCZOS)


def main():
    trunk_dark = (66, 48, 34)
    species = [
        finish(conifer((26, 52, 30), (90, 130, 66), trunk_dark, 11)),      # sapin
        finish(broadleaf((44, 82, 38), (120, 158, 78), trunk_dark, 22)),   # feuillu
        finish(larch((78, 108, 54), (168, 186, 108), (96, 74, 50), 33)),   # mélèze
    ]
    n = len(species)
    atlas = Image.new("RGBA", (CELL * n, CELL), (0, 0, 0, 0))
    for i, im in enumerate(species):
        atlas.paste(im, (i * CELL, 0))

    out_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__)))), "assets", "vegetation")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "trees_atlas.png")
    atlas.save(out_path)
    print("[vegetation] atlas écrit : {} ({}x{}, {} espèces)".format(
        out_path, CELL * n, CELL, n))


if __name__ == "__main__":
    main()
