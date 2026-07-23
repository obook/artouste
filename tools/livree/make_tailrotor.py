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

La rastérisation des triangles (peindre_triangles) est générique et vit dans
raster.py, réutilisable par d'autres pièces peintes depuis Blender.

Usage : blender --background --python tools/livree/make_tailrotor.py

Auteur : O. Booklage
Licence : GPL v2
"""

import sys
from pathlib import Path

import addon_utils
import bpy
import numpy as np

from raster import peindre_triangles

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/
from common.paths import assets_dir

TR = assets_dir("models", "Alouette-II", "Models", "Externals", "TailRotor")
BLADE_AC = str(TR / "blade.ac")
SRC_PNG = str(TR / "tailrotor.png")


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


def generer(tris, couleur_fn, sortie_png):
    """Charge la texture source, peint la pale (rastérisation générique de
       raster.py, t = envergure normalisée), enregistre vers sortie_png."""
    img = bpy.data.images.load(SRC_PNG)
    img.colorspace_settings.name = "sRGB"
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float64).reshape(h, w, 4)
    peindre_triangles(px, tris, couleur_fn)
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
    generer(tris, zebre, str(TR / "tailrotor-gendarmerie.png"))


if __name__ == "__main__":
    main()
