#!/usr/bin/env python3
"""
cuire_geometrie.py
Cuit sur le dépliage de l'Alouette II deux cartes qui décrivent la géométrie,
pas la lumière : la position (repère modèle) et la normale de chaque texel.
Avec elles, usure.py peint ensuite la salissure en NumPy, en coordonnées du
monde (suie derrière la turbine, coulures sur les flancs, crasse en bas),
sans relancer Blender à chaque réglage.

Blender ne sait pas cuire une position en PNG 8 bits : on la fait passer par
une émission, ramenée de [-5, 5] m à [0, 1] (4 cm par pas, assez pour de la
crasse). La normale passe de [-1, 1] à [0, 1]. Les deux images sont créées en
"Non-Color" : la valeur écrite est la valeur lue, sans courbe sRGB.

Chaîne, depuis la racine du dépôt (même OBJ que cuire_ao.py) :

    ./build/bin/model_probe assets/models/Alouette-II/Models/alouette.ac \\
        /tmp/alouette.obj "hdr,blur,disc,flotteur,barre,roue"
    flatpak run org.blender.Blender -b --factory-startup \\
        -P tools/livree/cuire_geometrie.py -- /tmp/alouette.obj /tmp/geo 2048

Sortie : <prefixe>-position.png et <prefixe>-normale.png, dans le repère de
l'OBJ (celui du .ac : nez vers -X, Y vers le haut), l'import OBJ étant
demandé sans changement d'axes.

Auteur : O. Booklage
Date : novembre 2026
Licence : GPL v2
"""

import sys

import bpy

TAILLE_DEFAUT = 2048
PORTEE_M = 5.0   # la cellule tient dans [-5, 5] m autour de l'origine
MARGE_PX = 16


def scene_vide():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def importer(chemin_obj):
    """Importe l'OBJ sans changer d'axes : les cartes gardent le repère du .ac."""
    bpy.ops.wm.obj_import(filepath=chemin_obj, forward_axis="Y", up_axis="Z")
    objet = bpy.context.selected_objects[0]
    bpy.context.view_layer.objects.active = objet
    return objet


def materiau_emission(objet, image, source, decalage, echelle):
    """Branche Geometry.<source> -> (v + decalage) * echelle -> Emission."""
    mat = bpy.data.materials.new("cuisson")
    mat.use_nodes = True
    noeuds = mat.node_tree.nodes
    liens = mat.node_tree.links
    noeuds.clear()
    geo = noeuds.new("ShaderNodeNewGeometry")
    add = noeuds.new("ShaderNodeVectorMath")
    add.operation = "ADD"
    add.inputs[1].default_value = (decalage, decalage, decalage)
    mul = noeuds.new("ShaderNodeVectorMath")
    mul.operation = "SCALE"
    mul.inputs["Scale"].default_value = echelle
    emission = noeuds.new("ShaderNodeEmission")
    sortie = noeuds.new("ShaderNodeOutputMaterial")
    cible = noeuds.new("ShaderNodeTexImage")
    cible.image = image
    noeuds.active = cible
    liens.new(geo.outputs[source], add.inputs[0])
    liens.new(add.outputs[0], mul.inputs[0])
    liens.new(mul.outputs[0], emission.inputs["Color"])
    liens.new(emission.outputs[0], sortie.inputs["Surface"])
    objet.data.materials.clear()
    objet.data.materials.append(mat)


def cuire(objet, taille, nom, source, decalage, echelle, chemin):
    image = bpy.data.images.new(nom, taille, taille)
    image.colorspace_settings.name = "Non-Color"
    materiau_emission(objet, image, source, decalage, echelle)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 1
    scene.cycles.device = "CPU"
    scene.cycles.use_denoising = False
    scene.render.bake.margin = MARGE_PX
    scene.render.bake.use_clear = True
    bpy.ops.object.bake(type="EMIT")
    image.filepath_raw = chemin
    image.file_format = "PNG"
    image.save()
    print("cuire_geometrie : %s -> %s" % (nom, chemin))


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(args) < 2:
        raise SystemExit("Usage : blender -b -P cuire_geometrie.py -- entree.obj prefixe [taille]")
    entree, prefixe = args[0], args[1]
    taille = int(args[2]) if len(args) > 2 else TAILLE_DEFAUT

    scene_vide()
    objet = importer(entree)
    print("cuire_geometrie : %d faces importées" % len(objet.data.polygons))
    cuire(objet, taille, "position", "Position", PORTEE_M, 0.5 / PORTEE_M,
          prefixe + "-position.png")
    cuire(objet, taille, "normale", "Normal", 1.0, 0.5, prefixe + "-normale.png")


if __name__ == "__main__":
    main()
