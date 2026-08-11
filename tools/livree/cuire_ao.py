#!/usr/bin/env python3
"""
cuire_ao.py
Cuit une carte d'occlusion ambiante de l'Alouette II sur son dépliage d'origine,
avec Blender en mode sans interface. L'occlusion ambiante est l'ombre que la
cellule se porte à elle-même : sous la poutre de queue, entre les tubes du
treillis, dans le creux du capot moteur. Le moteur ne la calcule pas (voir
model.frag, qui n'a qu'un demi-Lambert), d'où l'intérêt de la peindre une fois
pour toutes dans la texture.

Blender ne lit pas l'AC3D. On passe donc par un OBJ produit par model_probe,
qui réutilise le chargeur du simulateur, filtres compris -- il faut écarter les
mêmes nœuds qu'au rendu, sans quoi le disque flou du rotor couvrirait toute la
cellule d'une ombre qui n'existe pas :

    ./build/bin/model_probe assets/models/Alouette-II/Models/alouette.ac \\
        /tmp/alouette.obj "hdr,blur,disc,flotteur,barre,roue"

Usage :
    blender -b -P tools/livree/cuire_ao.py -- entree.obj sortie.png [taille] [occulteur.obj]

L'occulteur, facultatif, est une pièce qui fait de l'ombre sans être cuite : le
fuselage sert ainsi à ombrager l'intérieur de cabine (voir ajouter_occulteur).

La sortie se cuit en 1024 par défaut, soit le double de l'atlas peint : le
suréchantillonnage se fait à la réduction, dans appliquer_ao.py.

Auteur : O. Booklage
Date : août 2026
Licence : GPL v2
"""

import sys

import bpy

TAILLE_DEFAUT = 1024
ECHANTILLONS = 128
DISTANCE_M = 2.5  # portée des rayons d'occlusion, en mètres
MARGE_PX = 16     # débordement autour des îlots, contre les coutures


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


def ajouter_occulteur(chemin_obj, cible):
    """Ajoute une pièce qui fait de l'ombre sans être cuite elle-même.

       L'intérieur de cabine, cuit seul, ne s'occulte presque pas : ce sont le
       plancher, la cloison arrière et le capot moteur du fuselage qui
       l'ombragent. On importe donc le fuselage comme occulteur, puis on rend la
       sélection à la cible, seule pièce que la cuisson doit écrire. La verrière
       est à écarter de l'occulteur au moment de l'export : opaque à la cuisson,
       elle plongerait la cabine dans une ombre que le verre ne fait pas."""
    importer(chemin_obj)
    bpy.ops.object.select_all(action="DESELECT")
    cible.select_set(True)
    bpy.context.view_layer.objects.active = cible


def dimensions(texte):
    """Lit une taille "1024" ou "1024x512" et renvoie (largeur, hauteur).

       Tous les atlas ne sont pas carrés : celui de l'intérieur de cabine fait
       1024x512, et cuire carré gaspillerait la moitié de l'image."""
    if "x" in texte:
        largeur, hauteur = texte.split("x", 1)
        return int(largeur), int(hauteur)
    return int(texte), int(texte)


def preparer_cible(objet, taille):
    """Crée l'image de cuisson et la branche comme cible du matériau.

    Blender cuit dans le nœud de texture actif du matériau : sans ce montage,
    la cuisson n'a nulle part où écrire. La cuisson efface l'image en NOIR avant
    d'écrire, quoi qu'on ait demandé à la création : les texels qu'aucune face
    n'utilise ressortent donc noirs, et c'est à l'application de les ignorer
    (voir le seuil dans occlusion(), retint.py).
    """
    largeur, hauteur = taille
    image = bpy.data.images.new("occlusion", largeur, hauteur)

    materiau = bpy.data.materials.new("cuisson")
    materiau.use_nodes = True
    noeud = materiau.node_tree.nodes.new("ShaderNodeTexImage")
    noeud.image = image
    materiau.node_tree.nodes.active = noeud
    objet.data.materials.clear()
    objet.data.materials.append(materiau)
    return image


def cuire(scene, distance=DISTANCE_M):
    scene.render.engine = "CYCLES"
    scene.cycles.samples = ECHANTILLONS
    scene.cycles.device = "CPU"
    # Débruitage coupé : le paquet Blender de la distribution est compilé sans
    # OpenImageDenoiser. Le débruitage étant actif par défaut, la cuisson échoue
    # et rend une image entièrement noire, sans message d'erreur explicite.
    scene.cycles.use_denoising = False
    if scene.world is None:
        scene.world = bpy.data.worlds.new("monde")
    scene.world.light_settings.distance = distance
    scene.render.bake.margin = MARGE_PX
    scene.render.bake.use_clear = True
    bpy.ops.object.bake(type="AO")


USAGE = ("Usage : blender -b -P cuire_ao.py -- entree.obj sortie.png"
         " [taille] [occulteur.obj] [distance_m]")


def main():
    if "--" not in sys.argv:
        raise SystemExit(USAGE)
    args = sys.argv[sys.argv.index("--") + 1:]
    if len(args) < 2:
        raise SystemExit(USAGE)
    entree, sortie = args[0], args[1]
    taille = dimensions(args[2]) if len(args) > 2 else (TAILLE_DEFAUT, TAILLE_DEFAUT)
    occulteur = args[3] if len(args) > 3 and args[3] != "-" else None
    distance = float(args[4]) if len(args) > 4 else DISTANCE_M

    scene_vide()
    objet = importer(entree)
    print("cuire_ao : %d faces importées" % len(objet.data.polygons))
    image = preparer_cible(objet, taille)
    if occulteur is not None:
        ajouter_occulteur(occulteur, objet)
        print("cuire_ao : occulteur ajouté (%s)" % occulteur)
    cuire(bpy.context.scene, distance)

    image.filepath_raw = sortie
    image.file_format = "PNG"
    image.save()
    print("cuire_ao : occlusion écrite en %dx%d, rayons à %.2f m -> %s"
          % (taille[0], taille[1], distance, sortie))


if __name__ == "__main__":
    main()
