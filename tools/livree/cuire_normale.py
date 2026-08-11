#!/usr/bin/env python3
"""
cuire_normale.py
Cuit une carte de relief (normal map, espace tangent) de l'Alouette II sur son
dépliage d'origine, avec Blender en mode sans interface. Le relief vient d'une
copie chanfreinée et lissée du maillage : les arêtes vives des tôles prennent
un congé de quelques millimètres et les tubes du treillis deviennent ronds,
ce que la carte capture sans ajouter un seul sommet au modèle affiché.

La chaîne est celle de cuire_ao.py : un OBJ produit par model_probe, mêmes
filtres de nœuds qu'au rendu :

    ./build/bin/model_probe assets/models/Alouette-II/Models/alouette.ac \\
        /tmp/alouette.obj "hdr,blur,disc,flotteur,barre,roue"

Usage :
    blender -b -P tools/livree/cuire_normale.py -- entree.obj sortie.png [taille] [chanfrein_m]

Le chanfrein par défaut (8 mm) est taillé pour la cellule ; une pièce aux
détails fins comme la planche de bord se cuit avec un congé plus étroit.

Contrairement à la cuisson d'occlusion, la passe NORMAL n'est pas affectée par
l'absence d'OpenImageDenoiser ; le débruitage est tout de même coupé par
cohérence. La convention d'espace tangent n'a pas d'importance au branchement :
model.frag reconstruit le repère au pixel par les dérivées d'écran, la carte
est lue telle quelle. Le moteur associe de lui-même X-relief.png à toute
texture de couleur X.png posée à côté.

Auteur : O. Booklage
Date : août 2026
Licence : GPL v2
"""

import math
import sys

import bpy

TAILLE_DEFAUT = 2048
ECHANTILLONS = 16
CHANFREIN_M = 0.008   # largeur du congé sur les arêtes vives, en mètres
ANGLE_VIF = 30.0      # en deçà de cet angle dièdre, l'arête reste telle quelle
CAGE_M = 0.015        # débord de la cage de cuisson, au-delà du chanfrein
MARGE_PX = 16         # débordement autour des îlots, contre les coutures


def scene_vide():
    """Vide la scène de départ (cube, caméra, lampe)."""
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def importer(chemin_obj):
    """Importe l'OBJ et renvoie l'objet maillage, sélectionné et actif."""
    bpy.ops.wm.obj_import(filepath=chemin_obj)
    objet = bpy.context.selected_objects[0]
    bpy.context.view_layer.objects.active = objet
    return objet


def copie_chanfreinee(bas, chanfrein):
    """Duplique le maillage et lui donne le relief à cuire.

    L'OBJ arrive avec ses sommets dédoublés le long des coutures ; on les
    soude d'abord, sans quoi le biseau ne voit aucune arête à travailler.
    Puis biseau sur les arêtes vives et lissage : c'est cette version-là,
    jamais affichée, dont les normales seront reportées sur l'original.
    """
    bpy.ops.object.duplicate()
    haut = bpy.context.selected_objects[0]
    bpy.context.view_layer.objects.active = haut

    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.remove_doubles(threshold=1e-5)
    bpy.ops.object.mode_set(mode="OBJECT")

    biseau = haut.modifiers.new("chanfrein", "BEVEL")
    biseau.width = chanfrein
    biseau.segments = 2
    biseau.limit_method = "ANGLE"
    biseau.angle_limit = math.radians(ANGLE_VIF)

    bpy.ops.object.shade_smooth()
    haut.data.use_auto_smooth = True
    haut.data.auto_smooth_angle = math.radians(ANGLE_VIF)
    return haut


def preparer_cible(objet, taille):
    """Crée l'image de cuisson et la branche comme cible du matériau.

    Blender cuit dans le nœud de texture actif du matériau du maillage BAS,
    celui qui porte les UV d'origine. L'effacement de la cuisson remplit
    lui-même le fond de la couleur neutre (0.5, 0.5, 1) : un texel jamais
    écrit ne penche d'aucun côté.
    """
    image = bpy.data.images.new("relief", taille, taille)
    image.colorspace_settings.name = "Non-Color"

    materiau = bpy.data.materials.new("cuisson")
    materiau.use_nodes = True
    noeud = materiau.node_tree.nodes.new("ShaderNodeTexImage")
    noeud.image = image
    materiau.node_tree.nodes.active = noeud
    objet.data.materials.clear()
    objet.data.materials.append(materiau)
    return image


def cuire(scene):
    scene.render.engine = "CYCLES"
    scene.cycles.samples = ECHANTILLONS
    scene.cycles.device = "CPU"
    scene.cycles.use_denoising = False
    scene.render.bake.use_selected_to_active = True
    scene.render.bake.cage_extrusion = CAGE_M
    scene.render.bake.max_ray_distance = 2.0 * CAGE_M + CHANFREIN_M
    scene.render.bake.normal_space = "TANGENT"
    scene.render.bake.margin = MARGE_PX
    scene.render.bake.use_clear = True
    bpy.ops.object.bake(type="NORMAL")


def main():
    usage = ("Usage : blender -b -P cuire_normale.py -- "
             "entree.obj sortie.png [taille] [chanfrein_m]")
    if "--" not in sys.argv:
        raise SystemExit(usage)
    args = sys.argv[sys.argv.index("--") + 1:]
    if len(args) < 2:
        raise SystemExit(usage)
    entree, sortie = args[0], args[1]
    taille = int(args[2]) if len(args) > 2 else TAILLE_DEFAUT
    chanfrein = float(args[3]) if len(args) > 3 else CHANFREIN_M

    scene_vide()
    bas = importer(entree)
    print("cuire_normale : %d faces importées" % len(bas.data.polygons))
    haut = copie_chanfreinee(bas, chanfrein)
    image = preparer_cible(bas, taille)

    # Cuisson sélection-vers-actif : la copie en relief sélectionnée, l'original
    # actif. C'est lui qui reçoit la carte, sur ses UV.
    bpy.ops.object.select_all(action="DESELECT")
    haut.select_set(True)
    bas.select_set(True)
    bpy.context.view_layer.objects.active = bas
    cuire(bpy.context.scene)

    image.filepath_raw = sortie
    image.file_format = "PNG"
    image.save()
    print("cuire_normale : relief écrit en %dx%d -> %s" % (taille, taille, sortie))


if __name__ == "__main__":
    main()
