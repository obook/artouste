#!/usr/bin/env python3
"""
make_tailrotor.py
Peint le skin des pales du rotor de queue de l'Alouette II.
Blender importe blade.ac et fournit la géométrie (triangles : coordonnées UV +
position d'envergure de chaque sommet). On rastérise ensuite soi-même les
triangles de la pale sur une copie de la texture existante, en coloriant chaque
pixel selon sa position d'envergure : métal uni (livrée d'origine), ou bandes
jaune/rouge (Gendarmerie). Le moyeu et le disque, hors triangles de la pale, sont
préservés à l'identique.

Produit deux textures :
  - tailrotor.png             : pales métal nu (livrée d'origine) ;
  - tailrotor-gendarmerie.png : pales jaunes à zébrures rouges (Gendarmerie).

Usage : blender --background --python tools/livree/make_tailrotor.py

Auteur : O. Booklage
Licence : GPL v2
"""

import os

import addon_utils
import bpy
import numpy as np

RACINE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TR = os.path.join(RACINE, "assets", "models", "Alouette-II", "Models", "Externals", "TailRotor")
BLADE_AC = os.path.join(TR, "blade.ac")
SRC_PNG = os.path.join(TR, "tailrotor.png")


def srgb_vers_lineaire(c):
    """Convertit une couleur sRGB (composantes 0..1) en linéaire (espace des
       pixels Blender). Nécessaire pour écrire la bonne teinte dans l'image."""
    c = np.asarray(c, dtype=np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def importer_triangles():
    """Importe blade.ac et renvoie la liste des triangles de l'objet 'blade'.
       Chaque triangle = 3 sommets (u, v, envergure), envergure = X local."""
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    addon_utils.enable("io_scene_ac3d", default_set=True)
    bpy.ops.import_scene.import_ac3d(filepath=BLADE_AC)
    blade = bpy.data.objects.get("blade")
    if blade is None:
        raise RuntimeError("objet 'blade' introuvable dans blade.ac")
    me = blade.data
    me.calc_loop_triangles()
    uvl = me.uv_layers.active.data
    tris = []
    for lt in me.loop_triangles:
        sommets = []
        for i in range(3):
            uv = uvl[lt.loops[i]].uv
            span = me.vertices[lt.vertices[i]].co.x
            sommets.append((uv.x, uv.y, span))
        tris.append(sommets)
    return tris


def metal(_t):
    """Couleur (sRGB) d'un point de la pale en métal nu : gris alliage uni."""
    return (0.66, 0.67, 0.69)


def zebre(t):
    """Couleur (sRGB) d'un point de la pale Gendarmerie selon l'envergure
       normalisée t (0 = pied, 1 = bout) : fond jaune, bandes et bout rouges.
       Positions à affiner sur la photo de référence."""
    jaune = (0.96, 0.80, 0.06)
    rouge = (0.82, 0.07, 0.06)
    bandes_rouges = ((0.50, 0.60), (0.74, 0.82), (0.90, 1.01))  # bout rouge inclus
    for lo, hi in bandes_rouges:
        if lo <= t < hi:
            return rouge
    return jaune


def peindre_pale(px, tris, couleur_fn):
    """Rastérise chaque triangle de la pale dans le tableau de pixels px (H,W,4,
       linéaire, rangé bas-en-haut comme Blender). La couleur de chaque pixel est
       donnée par couleur_fn(envergure_normalisée)."""
    h, w, _ = px.shape
    spans = [s for tri in tris for (_, _, s) in tri]
    smin, smax = min(spans), max(spans)
    etendue = (smax - smin) if smax > smin else 1.0

    for tri in tris:
        # Coordonnées pixel (u -> x, v -> y depuis le bas, comme Blender).
        pts = [(u * w, v * h, s) for (u, v, s) in tri]
        (x0, y0, s0), (x1, y1, s1), (x2, y2, s2) = pts
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
        span = a * s0 + b * s1 + c * s2
        tnorm = (span - smin) / etendue
        # On colorie pixel par pixel les points intérieurs au triangle.
        rr, cc = np.nonzero(dedans)
        for k in range(rr.size):
            t = float(tnorm[rr[k], cc[k]])
            px[ys[rr[k], cc[k]], xs[rr[k], cc[k]], :3] = srgb_vers_lineaire(couleur_fn(t))
            px[ys[rr[k], cc[k]], xs[rr[k], cc[k]], 3] = 1.0


def generer(tris, couleur_fn, sortie_png):
    """Charge la texture source, peint la pale, enregistre vers sortie_png."""
    img = bpy.data.images.load(SRC_PNG)
    img.colorspace_settings.name = "sRGB"
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float64).reshape(h, w, 4)
    peindre_pale(px, tris, couleur_fn)
    img.pixels[:] = px.ravel()
    img.filepath_raw = sortie_png
    img.file_format = "PNG"
    img.save()
    print("[tailrotor] écrit", sortie_png)
    bpy.data.images.remove(img)


def main():
    tris = importer_triangles()
    print("[tailrotor] triangles de la pale :", len(tris))
    # 1) Pales métal -> écrase tailrotor.png (livrée d'origine).
    generer(tris, metal, SRC_PNG)
    # 2) Pales zébrées -> tailrotor-gendarmerie.png (Gendarmerie).
    generer(tris, zebre, os.path.join(TR, "tailrotor-gendarmerie.png"))


if __name__ == "__main__":
    main()
