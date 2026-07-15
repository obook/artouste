#!/usr/bin/env python3
"""
Fichier : blender_utils.py
Description : Fonctions utilitaires Blender pour la création des objets du
              tableau de bord SE 3130 Alouette II (collections, matériaux,
              cadrans, voyants, support).
Auteur : O. Booklage
"""

import bpy

from donnees import ECHELLE, EPAISSEUR_CADRAN, EPAISSEUR_TABLEAU, ORIGINE_X, ORIGINE_Y, ORIGINE_Z


def obtenir_ou_creer_collection(nom: str) -> bpy.types.Collection:
    """Crée la collection de destination et la lie à la scène si besoin."""
    collection = bpy.data.collections.get(nom)
    if collection is None:
        collection = bpy.data.collections.new(nom)
    # Une collection peut exister dans bpy.data sans être visible dans la
    # scène courante (délié dans l'Outliner, autre scène du .blend).
    racine = bpy.context.scene.collection
    if collection.name not in [c.name for c in racine.children_recursive]:
        racine.children.link(collection)
    return collection


def vider_collection(collection: bpy.types.Collection) -> None:
    """Supprime les objets d'une exécution précédente (relance propre)."""
    for obj in list(collection.objects):
        maillage = obj.data
        bpy.data.objects.remove(obj)
        if maillage and maillage.users == 0:
            bpy.data.meshes.remove(maillage)


def noeud_principled(mat: bpy.types.Material) -> bpy.types.ShaderNode:
    """Retrouve le noeud Principled BSDF par type (son nom peut être traduit)."""
    for noeud in mat.node_tree.nodes:
        if noeud.type == "BSDF_PRINCIPLED":
            return noeud
    return None


def creer_materiau(
    nom: str,
    couleur: tuple,
    emission_force: float = 0.0,
) -> bpy.types.Material:
    """Crée ou met à jour un matériau Principled BSDF avec la couleur donnée.

    Si le matériau existe déjà (relance du script), ses réglages sont
    réappliqués pour refléter les constantes actuelles du fichier.
    """
    mat = bpy.data.materials.get(nom)
    if mat is None:
        mat = bpy.data.materials.new(name=nom)
    mat.use_nodes = True
    noeud = noeud_principled(mat)
    if noeud:
        noeud.inputs["Base Color"].default_value = couleur
        noeud.inputs["Roughness"].default_value = 0.4
        noeud.inputs["Metallic"].default_value = 0.6
        if emission_force > 0.0:
            # Le socket s'appelle "Emission Color" en Blender 4.x
            # et "Emission" en Blender 3.x.
            entree = noeud.inputs.get("Emission Color")
            if entree is None:
                entree = noeud.inputs.get("Emission")
            if entree:
                entree.default_value = couleur
            force = noeud.inputs.get("Emission Strength")
            if force:
                force.default_value = emission_force
    return mat


def lier_objet_a_collection(
    obj: bpy.types.Object,
    collection: bpy.types.Collection,
) -> None:
    """Lie l'objet à la collection et le supprime des autres."""
    for coll in obj.users_collection:
        coll.objects.unlink(obj)
    collection.objects.link(obj)


def creer_cadran_circulaire(
    nom: str,
    rayon_mm: float,
    couleur: tuple,
    collection: bpy.types.Collection,
) -> bpy.types.Object:
    """Crée un cylindre plat représentant un cadran d'instrument."""
    rayon = rayon_mm * ECHELLE
    epaisseur = EPAISSEUR_CADRAN * ECHELLE

    bpy.ops.mesh.primitive_cylinder_add(
        vertices=48,
        radius=rayon,
        depth=epaisseur,
        enter_editmode=False,
        align="WORLD",
    )
    obj = bpy.context.active_object
    obj.name = nom

    mat = creer_materiau(f"mat_{nom}", couleur)
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)

    lier_objet_a_collection(obj, collection)
    return obj


def creer_voyant(
    nom: str,
    rayon_mm: float,
    couleur: tuple,
    collection: bpy.types.Collection,
) -> bpy.types.Object:
    """Crée un disque plat représentant un voyant d'alerte."""
    rayon = rayon_mm * ECHELLE
    epaisseur = 3.0 * ECHELLE

    bpy.ops.mesh.primitive_cylinder_add(
        vertices=24,
        radius=rayon,
        depth=epaisseur,
        enter_editmode=False,
        align="WORLD",
    )
    obj = bpy.context.active_object
    obj.name = nom

    # Les voyants émettent une légère lumière
    mat = creer_materiau(f"mat_{nom}", couleur, emission_force=0.5)
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)

    lier_objet_a_collection(obj, collection)
    return obj


def creer_support_tableau(
    collection: bpy.types.Collection,
) -> bpy.types.Object:
    """Crée le panneau de fond du tableau de bord."""
    largeur = 480.0 * ECHELLE
    hauteur = 400.0 * ECHELLE
    epaisseur = EPAISSEUR_TABLEAU * ECHELLE

    bpy.ops.mesh.primitive_cube_add(size=1.0, enter_editmode=False)
    obj = bpy.context.active_object
    obj.name = "tableau_support"
    obj.scale = (largeur / 2, epaisseur / 2, hauteur / 2)
    bpy.ops.object.transform_apply(scale=True)

    mat = creer_materiau(
        "mat_tableau_fond",
        (0.08, 0.08, 0.08, 1.0),
    )
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)

    # Position : centré sur l'origine tableau
    obj.location = (
        ORIGINE_X,
        ORIGINE_Y - epaisseur / 2,
        ORIGINE_Z,
    )

    lier_objet_a_collection(obj, collection)
    return obj
