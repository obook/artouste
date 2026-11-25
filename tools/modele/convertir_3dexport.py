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

Avec --poste, le poste du FBX est gardé (planche, consoles, commandes)
avec ses deux pilotes, en noeuds "equipage-droit" et "equipage-gauche", et la
tête du pilote à part ("tete-pilote"). Le moteur ne pose alors plus les
pièces FlightGear.

Usage :
    flatpak run org.blender.Blender -b --factory-startup \\
        -P tools/modele/convertir_3dexport.py -- Civil_FBX.FBX sortie.glb [--poste]

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
# sièges et le plancher restent. Object001/002 ne sont jamais repris : c'est
# une tige à boîtier pendue au plafond, juste devant les yeux du pilote (deux
# copies superposées), absente de l'appareil réel.
POSTE = [
    ((-0.7, -3.5, 0.5), (0.7, -3.3, 0.7)),    # palonniers
    ((-0.5, -3.2, 0.5), (0.5, -3.0, 1.1)),    # manches cycliques et leurs embases
    ((0.1, -2.9, 0.5), (0.2, -2.3, 1.05)),    # levier de collectif
    ((-0.7, -3.4, 0.45), (0.7, -2.9, 1.5)),   # planche de bord et consoles
]
# Toujours retirés, absents de l'appareil réel et gênants pour la vue : un
# îlot dont la boîte tient dans l'une de ces boîtes et dont le point le plus
# bas est sous 'bas' est supprimé.
RETIRES = [
    # Barre en U au milieu de la cabine : descend du plafond derrière les
    # têtes, passe entre les pilotes à hauteur d'épaule, remonte vers la
    # verrière, avec le boîtier et la ferrure fixés dessus. 'bas' épargne les
    # petites pièces de l'arceau central prises dans la même boîte.
    (((0.0, -3.1, 1.35), (0.07, -2.35, 1.9)), 1.55),
    # Berceau devant le copilote : deux tubes sur ressorts portant un boîtier
    # à objectif. Le plancher de la boîte épargne les boutons du manche.
    (((0.0, -2.85, 1.235), (0.66, -2.66, 1.52)), 1.52),
]
# Repris avec --poste : les pilotes (un seul objet pour les deux).
PILOTES = "Pilots"
# Hauteur du cou du pilote, en mètres dans le repère recalé (patins à -1,387) :
# au-dessus, sa tête et son casque vont dans le noeud "tete-pilote", que le
# moteur ne dessine pas en vue cockpit (la caméra est dedans).
COU = 0.02


def couper(obj, nom, prendre):
    """Déplace dans un nouvel objet 'nom' les faces de 'obj' dont le centre
    vérifie 'prendre'."""
    import bmesh
    copie = obj.copy()
    copie.data = obj.data.copy()
    copie.name = nom
    bpy.context.scene.collection.objects.link(copie)
    for o, garder in ((copie, True), (obj, False)):
        bm = bmesh.new()
        bm.from_mesh(o.data)
        jeter = [f for f in bm.faces if prendre(f.calc_center_median()) != garder]
        bmesh.ops.delete(bm, geom=jeter, context="FACES")
        bm.to_mesh(o.data)
        bm.free()


def scinder_equipage(pilotes):
    """Coupe l'objet des pilotes en un noeud par siège, et la tête du pilote
    à part. Dans le repère du FBX, x < 0 est le côté droit, celui du pilote."""
    couper(pilotes, "equipage-gauche", lambda c: c.x >= 0.0)
    couper(pilotes, "tete-pilote", lambda c: c.z > COU)
    pilotes.name = "equipage-droit"


def retirer_poste(fuse, avec_poste):
    """Supprime de 'fuse' les îlots de RETIRES et, sans 'avec_poste', ceux
    qui tiennent dans une boîte de POSTE."""
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

        def dedans(boite):
            return all(boite[0][i] <= mn[i] and mx[i] <= boite[1][i] for i in range(3))
        if any(dedans(b) and mn[2] < bas for b, bas in RETIRES) or \
                (not avec_poste and any(dedans(b) for b in POSTE)):
            a_supprimer.extend(ilot)
    faces = {f for v in a_supprimer for f in v.link_faces}
    # Console centrale : son boîtier est soudé au plancher, l'îlot déborde de
    # toute boîte ; on coupe ses faces une à une dans la zone.
    for f in bm.faces if not avec_poste else []:
        c = f.calc_center_median() * ECHELLE
        if abs(c.x) < 0.35 and -3.45 <= c.y <= -2.95 and 0.45 <= c.z <= 1.5:
            faces.add(f)
    bmesh.ops.delete(bm, geom=list(faces), context="FACES")
    bm.to_mesh(fuse.data)
    bm.free()
    print("convertir_3dexport : %d faces retirées du poste" % len(faces))


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    entree, sortie = args[:2]
    avec_poste = "--poste" in args[2:]
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    bpy.ops.import_scene.fbx(filepath=entree)
    for im in bpy.data.images:
        if "germany" in im.name:
            im.filepath = os.path.join(os.path.dirname(entree), "sa_313b.jpg")
            im.reload()

    # Ne garder que la cellule, en coordonnées monde (l'empty parent porte
    # l'échelle 0,01 et un quart de tour).
    noms = CELLULE + ([PILOTES] if avec_poste else [])
    garder = [o for o in bpy.data.objects if o.name in noms]
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
    pilotes = bpy.data.objects.get(PILOTES) if avec_poste else None
    retirer_poste(bpy.data.objects["fuse"], avec_poste)
    bpy.ops.object.select_all(action="DESELECT")
    for o in garder:
        o.select_set(o is not pilotes)
    bpy.context.view_layer.objects.active = bpy.data.objects["fuse"]
    bpy.ops.object.join()
    cellule = bpy.context.view_layer.objects.active
    cellule.name = "fuselage"
    # Mise à l'échelle et moyeu sur MAIN_HUB, dans le repère du FBX (x latéral,
    # y vers la queue, z haut). L'export glTF passe ensuite en Y haut, nez +Z :
    # (x, y, z) -> (x, z, -y), et le moteur fait le quart de tour restant.
    decalage = MOYEU_AC - MOYEU_FBX
    recalage = mathutils.Matrix.Translation(decalage) @ mathutils.Matrix.Scale(ECHELLE, 4)
    cellule.data.transform(recalage)
    if pilotes:
        pilotes.data.transform(recalage)
        scinder_equipage(pilotes)

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
        if o != cellule and not o.name.startswith(("equipage", "tete-pilote")):
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
