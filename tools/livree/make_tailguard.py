#!/usr/bin/env python3
"""
make_tailguard.py
Isole l'arceau de protection du rotor de queue de l'Alouette II, fondu dans l'objet
'structure' de alouette.ac, pour pouvoir le peindre selon la livrée (jaune en
Gendarmerie, gris métal en origine). Génère aussi les deux textures unies de
l'arceau.

Transformation (à lancer sur l'alouette.ac ORIGINAL ; restaurer d'abord depuis git
si on régénère : git checkout <commit-origine> -- .../Models/alouette.ac) :
  alouette.ac (original)
    -> alouette.ac        : fuselage SANS l'arceau (la D-hoop est retirée) ;
    -> tailguard.ac       : la D-hoop seule (pièce séparée, rechargée par le moteur) ;
    -> tailguard-gendarmerie.png : jaune uni (232,154,1) ;
    -> tailguard-origine.png     : gris métal uni (100,104,111).

Piège : à l'export, le greffon AC3D perd les textures si l'image n'est pas chargée
dans les matériaux ; on charge donc texture.png avant d'exporter (voir
assets/models/Alouette-II.md).

Usage : blender --background --python tools/livree/make_tailguard.py

Auteur : O. Booklage
Licence : GPL v2
"""

import os

import addon_utils
import bmesh
import bpy
import numpy as np

RACINE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MODELS = os.path.join(RACINE, "assets", "models", "Alouette-II", "Models")
ALOUETTE = os.path.join(MODELS, "alouette.ac")
TEX = os.path.join(MODELS, "texture.png")
SEUIL_X = 2.8  # au-delà en X (vers la queue), c'est la D-hoop ; en deçà, le tube avant


def texture_unie(nom, couleur):
    """Crée une petite texture PNG d'une couleur unie (donnée en octets 0..255).
       Image en 'Non-Color' : les pixels sont écrits tels quels (pas de conversion
       sRGB <-> linéaire), donc le PNG vaut exactement la couleur demandée."""
    img = bpy.data.images.new(nom, 8, 8, alpha=False)
    img.colorspace_settings.name = "Non-Color"
    r, g, b = (c / 255.0 for c in couleur)
    img.pixels[:] = np.tile([r, g, b, 1.0], 8 * 8)
    img.filepath_raw = os.path.join(MODELS, nom)
    img.file_format = "PNG"
    img.save()
    print("[tailguard] écrit", nom)


def importer():
    """Importe alouette.ac, charge texture.png dans les matériaux (sinon l'export
       perd les textures), et sépare 'structure' en parties connexes."""
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    addon_utils.enable("io_scene_ac3d", default_set=True)
    bpy.ops.import_scene.import_ac3d(filepath=ALOUETTE)
    img = bpy.data.images.load(TEX)
    for mat in bpy.data.materials:
        if not mat.use_nodes:
            continue
        for node in mat.node_tree.nodes:
            if node.type == "TEX_IMAGE":
                node.image = img
    st = bpy.data.objects.get("structure")
    bpy.ops.object.select_all(action="DESELECT")
    st.select_set(True)
    bpy.context.view_layer.objects.active = st
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.separate(type="LOOSE")
    bpy.ops.object.mode_set(mode="OBJECT")


def piece_arceau():
    """Repère la pièce connexe de l'arceau : proche de la queue (Xmax > 4), mince
       en Z (centrée) et descendant bas en Y."""
    for o in bpy.data.objects:
        if not o.name.startswith("structure"):
            continue
        vs = np.array([list(v.co) for v in o.data.vertices])
        if vs[:, 0].max() > 4.0 and (vs[:, 2].max() - vs[:, 2].min()) < 0.1 and vs[:, 1].min() < -0.5:
            return o
    raise RuntimeError("pièce de l'arceau introuvable")


def separer_hoop(piece):
    """Sépare les faces de la D-hoop (centre de face en X > SEUIL_X) en un nouvel
       objet, renvoie cet objet."""
    avant = set(o.name for o in bpy.data.objects)
    bpy.ops.object.select_all(action="DESELECT")
    piece.select_set(True)
    bpy.context.view_layer.objects.active = piece
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(piece.data)
    for f in bm.faces:
        f.select = f.calc_center_median().x > SEUIL_X
    bmesh.update_edit_mesh(piece.data)
    bpy.ops.mesh.separate(type="SELECTED")
    bpy.ops.object.mode_set(mode="OBJECT")
    return [o for o in bpy.data.objects if o.name not in avant][0]


def main():
    # tailguard.ac : la D-hoop seule (texturée).
    importer()
    hoop = separer_hoop(piece_arceau())
    for o in list(bpy.data.objects):
        if o.type == "MESH" and o is not hoop:
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.export_ac3d(filepath=os.path.join(MODELS, "tailguard.ac"))
    print("[tailguard] écrit tailguard.ac")

    # alouette.ac : le fuselage SANS la D-hoop (texture préservée).
    importer()
    hoop = separer_hoop(piece_arceau())
    bpy.data.objects.remove(hoop, do_unlink=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.export_ac3d(filepath=ALOUETTE)
    print("[tailguard] écrit alouette.ac (sans arceau)")

    # Textures unies de l'arceau (jaune Gendarmerie, gris métal origine).
    texture_unie("tailguard-gendarmerie.png", (232, 154, 1))
    texture_unie("tailguard-origine.png", (100, 104, 111))


if __name__ == "__main__":
    main()
