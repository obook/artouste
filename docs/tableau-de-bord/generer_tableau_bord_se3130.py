#!/usr/bin/env python3
"""
Fichier : generer_tableau_bord_se3130.py
Description : Génère le tableau de bord SE 3130 Alouette II dans Blender.
              À exécuter depuis Scripting > Run Script dans Blender.
              Crée les instruments comme objets séparés, positionnés d'après
              le Flight Manual Sud Aviation (SGAC approved, figure 2-5a)
              et la photo JetPhotos (Paulo Antunes).
Auteur : O. Booklage
Date : juin 2026
Usage : Ouvrir Blender, onglet Scripting, coller ce fichier, Run Script.
        Les objets sont créés dans la collection "Tableau_SE3130".
"""

import bpy
import math


# ---------------------------------------------------------------------------
# Constantes de mise à l'échelle
# ---------------------------------------------------------------------------

# Échelle : 1 unité Blender = 1 mm réel
# Le tableau de bord mesure environ 480 mm de large en réalité.
# Adapter ECHELLE si le modèle .glb d'helijah (fichier externe, hors dépôt)
# utilise une autre échelle.
# Pour vérifier : mesurer la largeur du fuselage dans Blender et comparer
# aux 2,07 m réels de l'Alouette II.
ECHELLE = 0.001  # 1 mm = 0.001 m (convention Blender mètres)

# Position du centre du tableau dans le repère du modèle .glb.
# À ajuster après import du tableau existant : placer ces coordonnées
# au centre géométrique du mesh à remplacer.
ORIGINE_X = 0.0    # décalage latéral (0 = axe de symétrie)
ORIGINE_Y = 0.5    # profondeur (vers l'avant du cockpit)
ORIGINE_Z = 1.2    # hauteur (pilote assis côté droit)

# Épaisseur des cadrans d'instruments (en mm)
EPAISSEUR_CADRAN = 15.0
EPAISSEUR_TABLEAU = 5.0


# ---------------------------------------------------------------------------
# Définitions des instruments (positions en mm, origine = centre tableau)
# ---------------------------------------------------------------------------
# Coordonnées (x, y) dans le plan du tableau, z = profondeur.
# x positif = vers la droite vu du pilote.
# y positif = vers le haut.
# Source : Flight Manual figure 2-5a + estimation photo JetPhotos.
# ATTENTION : cette table est recopiée dans generer_panel_se3130.py
# (branche panel-se3130-forme) ; toute retouche de position doit y être
# reportée pour que les deux générateurs restent d'accord.

INSTRUMENTS = [
    # -------------------------------------------------------------------
    # Zone A - instruments de vol principaux (rangée haute)
    # -------------------------------------------------------------------
    {
        "nom": "compas_magnetique",
        "label": "Compas",
        "item_flight_manual": 3,
        "x_mm": 0.0,
        "y_mm": 140.0,
        "rayon_mm": 35.0,
        "couleur": (0.15, 0.15, 0.15, 1.0),
        "zone": "A",
    },
    {
        "nom": "tachymetre_double",
        "label": "Tachy rotor+turbine",
        "item_flight_manual": 8,
        "x_mm": -130.0,
        "y_mm": 80.0,
        "rayon_mm": 52.0,  # le plus grand cadran du tableau
        "couleur": (0.1, 0.1, 0.12, 1.0),
        "zone": "A",
    },
    {
        "nom": "indicateur_vitesse",
        "label": "IAS 20-140 kt",
        "item_flight_manual": 40,
        "x_mm": -20.0,
        "y_mm": 80.0,
        "rayon_mm": 44.0,
        "couleur": (0.1, 0.1, 0.12, 1.0),
        "zone": "A",
    },
    {
        "nom": "altimetre",
        "label": "Altimètre ft",
        "item_flight_manual": 41,
        "x_mm": 110.0,
        "y_mm": 80.0,
        "rayon_mm": 44.0,
        "couleur": (0.1, 0.1, 0.12, 1.0),
        "zone": "A",
    },
    # -------------------------------------------------------------------
    # Zone B - instruments moteur et variomètre (rangée centrale)
    # -------------------------------------------------------------------
    {
        "nom": "variometre",
        "label": "Vario m/s",
        "item_flight_manual": 7,
        "x_mm": -130.0,
        "y_mm": -10.0,
        "rayon_mm": 40.0,
        "couleur": (0.1, 0.1, 0.12, 1.0),
        "zone": "B",
    },
    {
        "nom": "unite_triple",
        "label": "Triple: tuyère+huile",
        "item_flight_manual": 10,
        "x_mm": 30.0,
        "y_mm": -10.0,
        "rayon_mm": 56.0,  # le plus grand instrument
        "couleur": (0.1, 0.1, 0.12, 1.0),
        "zone": "B",
    },
    {
        "nom": "indicateur_collectif",
        "label": "Pas collectif 0-15deg",
        "item_flight_manual": 42,
        "x_mm": 155.0,
        "y_mm": -10.0,
        "rayon_mm": 32.0,
        "couleur": (0.1, 0.1, 0.12, 1.0),
        "zone": "B",
    },
    {
        "nom": "jauge_carburant",
        "label": "Carburant US gal",
        "item_flight_manual": 47,
        "x_mm": -190.0,
        "y_mm": -10.0,
        "rayon_mm": 28.0,
        "couleur": (0.1, 0.1, 0.12, 1.0),
        "zone": "B",
    },
]

# Voyants d'alerte (simples disques colorés)
VOYANTS = [
    {
        "nom": "voyant_huile_transmission",
        "item_flight_manual": 6,
        "x_mm": -160.0,
        "y_mm": -80.0,
        "rayon_mm": 8.0,
        "couleur": (0.8, 0.05, 0.05, 1.0),  # rouge
    },
    {
        "nom": "voyant_pression_carburant",
        "item_flight_manual": 9,
        "x_mm": -130.0,
        "y_mm": -80.0,
        "rayon_mm": 8.0,
        "couleur": (0.8, 0.05, 0.05, 1.0),  # rouge
    },
    {
        "nom": "voyant_demarreur_rouge",
        "item_flight_manual": 17,
        "x_mm": -100.0,
        "y_mm": -80.0,
        "rayon_mm": 8.0,
        "couleur": (0.8, 0.05, 0.05, 1.0),  # rouge
    },
    {
        "nom": "voyant_demarreur_vert",
        "item_flight_manual": 19,
        "x_mm": -70.0,
        "y_mm": -80.0,
        "rayon_mm": 8.0,
        "couleur": (0.05, 0.7, 0.05, 1.0),  # vert
    },
    {
        "nom": "voyant_micropompe",
        "item_flight_manual": 20,
        "x_mm": -40.0,
        "y_mm": -80.0,
        "rayon_mm": 8.0,
        "couleur": (0.9, 0.5, 0.0, 1.0),  # orange
    },
    {
        "nom": "voyant_generateur",
        "item_flight_manual": 48,
        "x_mm": -10.0,
        "y_mm": -80.0,
        "rayon_mm": 8.0,
        "couleur": (0.8, 0.05, 0.05, 1.0),  # rouge
    },
]


# ---------------------------------------------------------------------------
# Utilitaires Blender
# ---------------------------------------------------------------------------

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


# ---------------------------------------------------------------------------
# Génération principale
# ---------------------------------------------------------------------------

def generer_tableau_bord() -> None:
    """Point d'entrée principal : génère tous les objets du tableau."""
    print("[SE3130] Début de la génération du tableau de bord.")

    # Les opérateurs de création exigent le mode Objet (un Run Script
    # lancé depuis le mode Édition corromprait le mesh en cours d'édition).
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    collection = obtenir_ou_creer_collection("Tableau_SE3130")

    # Relance propre : on repart de zéro à chaque exécution du script
    vider_collection(collection)

    # Support de fond
    creer_support_tableau(collection)

    # Instruments (cadrans circulaires)
    for instr in INSTRUMENTS:
        nom = instr["nom"]
        obj = creer_cadran_circulaire(
            nom=nom,
            rayon_mm=instr["rayon_mm"],
            couleur=instr["couleur"],
            collection=collection,
        )
        # Positionnement dans l'espace du cockpit
        # Le cadran est orienté face au pilote (normal vers -Y cockpit)
        # On place le cylindre debout puis on le fait pivoter.
        x = ORIGINE_X + instr["x_mm"] * ECHELLE
        y = ORIGINE_Y
        z = ORIGINE_Z + instr["y_mm"] * ECHELLE

        obj.location = (x, y, z)
        # Rotation pour que la face soit visible depuis le siège pilote
        obj.rotation_euler = (math.radians(90), 0.0, 0.0)

        print(
            f"[SE3130] Instrument '{nom}' (item {instr['item_flight_manual']}) "
            f"placé en ({x:.3f}, {y:.3f}, {z:.3f})"
        )

    # Voyants d'alerte
    for voyant in VOYANTS:
        nom = voyant["nom"]
        obj = creer_voyant(
            nom=nom,
            rayon_mm=voyant["rayon_mm"],
            couleur=voyant["couleur"],
            collection=collection,
        )
        x = ORIGINE_X + voyant["x_mm"] * ECHELLE
        y = ORIGINE_Y
        z = ORIGINE_Z + voyant["y_mm"] * ECHELLE

        obj.location = (x, y, z)
        obj.rotation_euler = (math.radians(90), 0.0, 0.0)

        print(
            f"[SE3130] Voyant '{nom}' (item {voyant['item_flight_manual']}) "
            f"placé en ({x:.3f}, {y:.3f}, {z:.3f})"
        )

    print(
        f"[SE3130] Génération terminée : "
        f"{len(INSTRUMENTS)} instruments + {len(VOYANTS)} voyants."
    )
    print(
        "[SE3130] Vérifier et ajuster ORIGINE_X/Y/Z pour aligner "
        "avec le modèle .glb d'helijah."
    )


# ---------------------------------------------------------------------------
# Exécution
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    generer_tableau_bord()
