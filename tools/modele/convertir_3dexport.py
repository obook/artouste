#!/usr/bin/env python3
"""
convertir_3dexport.py
Convertit le modèle commercial 3dexport de l'Alouette II (Civil_FBX.FBX, hors
dépôt) en un .glb de cellule que le moteur charge à la place d'alouette.ac
(variable ARTOUSTE_MODELE). Seule la cellule est convertie : rotors, pilotes et
intérieur restent ceux du modèle FlightGear, posés par le moteur.

Le FBX est en centimètres, nez vers -Y, Z vers le haut. On le ramène en mètres
dans le repère du .ac (nez -X, Y haut, Z latéral), patins au sol et moyeu à
l'aplomb de MAIN_HUB (-2,142 ; 1,0 ; 0). Puis l'export glTF de Blender
tourne le tout dans sa convention (Y haut, nez +Z), que le moteur retourne
d'un quart de tour au dessin.

Les faces du matériau "glass" sont détachées dans un objet "verriere" : le
moteur rend translucides les noeuds dont le nom contient "verriere".
La texture du rotor (sa_313b_germany_c.jpg) manque de l'archive : on la
remplace par l'atlas civil, même dépliage.

Usage :
    flatpak run org.blender.Blender -b --factory-startup \\
        -P tools/modele/convertir_3dexport.py -- Civil_FBX.FBX sortie.glb

Auteur : O. Booklage
Date : novembre 2026
Licence : GPL v2
"""

import os
import sys

import bpy
import mathutils

ECHELLE = 100.0
# Moyeu du FBX en mètres, une fois mis à l'échelle (origine de Blurred_Main_Prop).
MOYEU_FBX = mathutils.Vector((0.04, -0.79, 2.55))
# Cible dans le repère du .ac : (latéral, longitudinal, haut) = (Z, X, Y).
# Latéral, X_ac, Y_ac. Le moyeu monte de 16 cm au-dessus du plan des pales du
# jeu (1,003) pour que les patins posent à -1,387 comme ceux du .ac ; le moyeu
# du jeu (mainrotor.ac) descend assez pour couvrir le raccord.
MOYEU_AC = mathutils.Vector((0.0, -2.142, 1.164))
CELLULE = ["fuse", "tail", "elevator", "gear_c", "gear_l", "gear_r",
           "blister1", "blister2"]
# Meubles du poste à retirer de "fuse" (le jeu pose les siens, animés) : un
# îlot du maillage dont la boîte tient dans l'une de ces boîtes, en mètres dans
# le repère du FBX (x latéral, y vers la queue, z haut), est supprimé. Les
# sièges et le plancher restent. Object001/002 (consoles de gauche) ne sont
# pas repris non plus.
POSTE = [
    ((-0.7, -3.5, 0.5), (0.7, -3.3, 0.7)),    # palonniers
    ((-0.5, -3.2, 0.5), (0.5, -3.0, 1.1)),    # manches cycliques et leurs embases
    ((0.1, -2.9, 0.5), (0.2, -2.3, 1.05)),    # levier de collectif
    ((-0.7, -3.4, 0.45), (0.7, -2.9, 1.5)),   # planche de bord et consoles
]


def retirer_poste(fuse):
    """Supprime les îlots de 'fuse' qui tiennent dans une boîte de POSTE."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(fuse.data)
    bm.verts.ensure_lookup_table()
    vus = set()
    a_supprimer = []
    for v0 in bm.verts:
        if v0.index in vus:
            continue
        ilot = [v0]
        vus.add(v0.index)
        pile = [v0]
        while pile:
            v = pile.pop()
            for e in v.link_edges:
                w = e.other_vert(v)
                if w.index not in vus:
                    vus.add(w.index)
                    ilot.append(w)
                    pile.append(w)
        # Sommets déjà cuits en repère monde ; matrix_world, elle, n'est pas
        # encore rafraîchie à ce stade et garderait l'échelle 0,01 du parent.
        pts = [v.co * ECHELLE for v in ilot]
        mn = [min(p[i] for p in pts) for i in range(3)]
        mx = [max(p[i] for p in pts) for i in range(3)]
        for (b0, b1) in POSTE:
            if all(b0[i] <= mn[i] and mx[i] <= b1[i] for i in range(3)):
                a_supprimer.extend(ilot)
                break
    faces = {f for v in a_supprimer for f in v.link_faces}
    # Console centrale : son boîtier est soudé au plancher, l'îlot déborde de
    # toute boîte ; on coupe ses faces une à une dans la zone.
    for f in bm.faces:
        c = f.calc_center_median() * ECHELLE
        if abs(c.x) < 0.35 and -3.45 <= c.y <= -2.95 and 0.45 <= c.z <= 1.5:
            faces.add(f)
    bmesh.ops.delete(bm, geom=list(faces), context="FACES")
    bm.to_mesh(fuse.data)
    bm.free()
    print("convertir_3dexport : poste du FBX retiré, %d faces" % len(faces))


def main():
    entree, sortie = sys.argv[sys.argv.index("--") + 1:][:2]
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    bpy.ops.import_scene.fbx(filepath=entree)
    for im in bpy.data.images:
        if "germany" in im.name:
            im.filepath = os.path.join(os.path.dirname(entree), "sa_313b.jpg")
            im.reload()

    # Ne garder que la cellule, en coordonnées monde (l'empty parent porte
    # l'échelle 0,01 et un quart de tour).
    garder = [o for o in bpy.data.objects if o.name in CELLULE]
    for o in garder:
        o.data.transform(o.matrix_world)
        o.parent = None
        # Pas matrix_world = identité : l'export relirait position et rotation
        # locales encore en place et les écrirait dans le noeud glTF.
        o.location = (0.0, 0.0, 0.0)
        o.rotation_mode = "QUATERNION"
        o.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        o.rotation_mode = "XYZ"
        o.rotation_euler = (0.0, 0.0, 0.0)
        o.scale = (1.0, 1.0, 1.0)
        o.delta_location = (0.0, 0.0, 0.0)
        o.delta_rotation_euler = (0.0, 0.0, 0.0)
        o.delta_rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        o.delta_scale = (1.0, 1.0, 1.0)
        o.matrix_parent_inverse = mathutils.Matrix.Identity(4)
        # Le FBX porte une animation (pose à la première image) que l'export
        # glTF écrirait dans le noeud, par-dessus la matrice identité.
        o.animation_data_clear()
    for o in list(bpy.data.objects):
        if o not in garder:
            bpy.data.objects.remove(o)
    retirer_poste(bpy.data.objects["fuse"])
    bpy.ops.object.select_all(action="SELECT")
    bpy.context.view_layer.objects.active = garder[0]
    bpy.ops.object.join()
    cellule = bpy.context.view_layer.objects.active
    cellule.name = "fuselage"
    # Mise à l'échelle et moyeu sur MAIN_HUB, dans le repère du FBX (x latéral,
    # y vers la queue, z haut). L'export glTF passe ensuite en Y haut, nez +Z :
    # (x, y, z) -> (x, z, -y), et le moteur fait le quart de tour restant.
    decalage = MOYEU_AC - MOYEU_FBX
    cellule.data.transform(mathutils.Matrix.Translation(decalage) @
                           mathutils.Matrix.Scale(ECHELLE, 4))

    # Vitrages à part.
    idx = [i for i, m in enumerate(cellule.data.materials) if m and m.name == "glass"]
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    for p in cellule.data.polygons:
        p.select = p.material_index in idx
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.separate(type="SELECTED")
    bpy.ops.object.mode_set(mode="OBJECT")
    for o in bpy.data.objects:
        if o != cellule:
            o.name = "verriere"
    print("convertir_3dexport : cellule %d faces, verrière %d faces" %
          (len(cellule.data.polygons), len(bpy.data.objects["verriere"].data.polygons)))

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=sortie, export_format="GLB", use_selection=True,
                              export_apply=True, export_image_format="JPEG", export_yup=True,
                              export_animations=False)
    print("convertir_3dexport : écrit", sortie)


if __name__ == "__main__":
    main()
