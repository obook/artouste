#!/usr/bin/env python3
"""
Fichier : generer_tableau_bord_se3130.py
Description : Génère le tableau de bord SE 3130 Alouette II dans Blender.
              Crée les instruments comme objets séparés, positionnés d'après
              le Flight Manual Sud Aviation (SGAC approved, figure 2-5a)
              et la photo JetPhotos (Paulo Antunes).
Auteur : O. Booklage
Date : juin 2026
Usage : Dans Blender, onglet Scripting, Text > Open, puis ouvrir ce fichier
        DEPUIS LE DISQUE (pas de copier-coller : les imports locaux vers
        donnees.py et blender_utils.py, restés dans ce même dossier
        "generateur", ont besoin d'un fichier réellement présent sur disque
        pour être résolus). Run Script.
        Les objets sont créés dans la collection "Tableau_SE3130".
"""

import sys
from pathlib import Path

# Permet d'importer donnees.py et blender_utils.py, situés dans le même
# dossier que ce script, quel que soit le répertoire de travail de Blender.
sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy
import math

from donnees import ECHELLE, INSTRUMENTS, ORIGINE_X, ORIGINE_Y, ORIGINE_Z, VOYANTS
from blender_utils import (
    creer_cadran_circulaire,
    creer_support_tableau,
    creer_voyant,
    obtenir_ou_creer_collection,
    vider_collection,
)


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
