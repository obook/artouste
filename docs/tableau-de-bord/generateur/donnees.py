#!/usr/bin/env python3
"""
Fichier : donnees.py
Description : Constantes de mise à l'échelle et positions des instruments et
              voyants du tableau de bord SE 3130 Alouette II, utilisées par
              generer_tableau_bord_se3130.py.
Auteur : O. Booklage
"""

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
