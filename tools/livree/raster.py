"""
raster.py
Rastérisation générique de triangles UV sur un tableau de pixels Blender
(linéaire, rangé bas-en-haut) : sert à peindre une géométrie importée depuis
Blender (par exemple les pales du rotor de queue, voir make_tailrotor.py) sans
dépendre d'un shader, en coloriant chaque pixel intérieur à un triangle selon
une fonction de couleur donnée par l'appelant.

Auteur : O. Booklage
Licence : GPL v2
"""

import numpy as np


def srgb_vers_lineaire(c):
    """Convertit une couleur sRGB (composantes 0..1) en linéaire (espace des
       pixels Blender). Nécessaire pour écrire la bonne teinte dans l'image."""
    c = np.asarray(c, dtype=np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def peindre_triangles(px, tris, couleur_fn):
    """Rastérise chaque triangle (liste de 3 sommets (u, v, t), t = paramètre
       scalaire libre transmis à couleur_fn, par exemple une position
       d'envergure) dans le tableau de pixels px (H, W, 4, linéaire, rangé
       bas-en-haut comme Blender). La couleur de chaque pixel est donnée par
       couleur_fn(t_normalisé), t_normalisé étant le paramètre t interpolé au
       pixel puis ramené dans [0, 1] sur l'étendue de tous les triangles."""
    h, w, _ = px.shape
    valeurs_t = [t for tri in tris for (_, _, t) in tri]
    tmin, tmax = min(valeurs_t), max(valeurs_t)
    etendue = (tmax - tmin) if tmax > tmin else 1.0

    for tri in tris:
        # Coordonnées pixel (u -> x, v -> y depuis le bas, comme Blender).
        pts = [(u * w, v * h, t) for (u, v, t) in tri]
        (x0, y0, t0), (x1, y1, t1), (x2, y2, t2) = pts
        minx = max(0, int(np.floor(min(x0, x1, x2))))
        maxx = min(w - 1, int(np.ceil(max(x0, x1, x2))))
        miny = max(0, int(np.floor(min(y0, y1, y2))))
        maxy = min(h - 1, int(np.ceil(max(y0, y1, y2))))
        if maxx < minx or maxy < miny:
            continue
        ys, xs = np.mgrid[miny:maxy + 1, minx:maxx + 1]
        cx = xs + 0.5
        cy = ys + 0.5
        denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(denom) < 1e-9:
            continue
        a = ((y1 - y2) * (cx - x2) + (x2 - x1) * (cy - y2)) / denom
        b = ((y2 - y0) * (cx - x2) + (x0 - x2) * (cy - y2)) / denom
        c = 1.0 - a - b
        dedans = (a >= 0) & (b >= 0) & (c >= 0)
        if not dedans.any():
            continue
        valeur = a * t0 + b * t1 + c * t2
        tnorm = (valeur - tmin) / etendue
        # On colorie pixel par pixel les points intérieurs au triangle.
        rr, cc = np.nonzero(dedans)
        for k in range(rr.size):
            t = float(tnorm[rr[k], cc[k]])
            px[ys[rr[k], cc[k]], xs[rr[k], cc[k]], :3] = srgb_vers_lineaire(couleur_fn(t))
            px[ys[rr[k], cc[k]], xs[rr[k], cc[k]], 3] = 1.0
