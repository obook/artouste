# -*- coding: utf-8 -*-
"""
bordeaux
Bordeaux et son agglomération : la Garonne et le port de la Lune au centre,
de l'aéroport de Mérignac et Pessac à l'ouest jusqu'à Cenon et Lormont sur la
rive droite, et de Villenave-d'Ornon au sud jusqu'au stade Matmut Atlantique et
Bruges au nord. Terre intérieure : pas d'océan, la Garonne garde sa vraie
imagerie (pas de recoloration de mer).
"""

ZONE = {
    "bbox": (-0.74, -0.42, 44.72, 44.96),
    "recolor_sea": False,
    "start": (-0.6964, 44.8362),  # aéroport de Bordeaux-Mérignac, vaste replat
    # Ville très dense : on relève le seuil à 5 m pour écarter garages, abris et
    # petites annexes, et garder un buildings.bin raisonnable (le détail à 2 m
    # pesait près de 30 Mo).
    "height_min": 5.0,
    "grid": 1024,      # mailles ~24,7 m au lieu de ~49,3 m (défaut 512)
    "ortho_px": 5000,  # ville : ortho ~5,3 m/px au lieu de ~24 (limite serveur IGN : 5010)
    "title": "Bordeaux (la Garonne, port de la Lune, Mérignac, Pessac)",
    "landmarks": [
        ("Bordeaux", -0.5757, 44.8378),
        ("Place de la Bourse", -0.5697, 44.8412),
        ("Cité du Vin", -0.5508, 44.8625),
        ("Pont de pierre", -0.5648, 44.8385),
        ("Mérignac", -0.6470, 44.8430),
        ("Pessac", -0.6311, 44.8067),
        ("Talence", -0.5912, 44.8080),
        ("Bègles", -0.5478, 44.8083),
        ("Cenon", -0.5300, 44.8560),
        ("Lormont", -0.5180, 44.8740),
        ("Le Bouscat", -0.5972, 44.8650),
        ("Bruges", -0.6005, 44.8775),
        ("Stade Matmut Atlantique", -0.5614, 44.8956),
        ("Aéroport de Mérignac", -0.6964, 44.8362),
        # Lieux ajoutés (coordonnées géocodeur IGN).
        ("Cathédrale Saint-André", -0.5779, 44.8376),
        ("Gare Saint-Jean", -0.5564, 44.8262),
        ("Pont Chaban-Delmas", -0.5517, 44.8581),
        ("Pont d'Aquitaine", -0.5449, 44.8814),
        ("Gradignan", -0.6125, 44.7686),
        ("Floirac", -0.5211, 44.8325),
        ("Villenave-d'Ornon", -0.5579, 44.7724),
    ],
    # Aéroport et hôpitaux dotés d'une aire de poser (coordonnées relevées sur
    # la carte ; à affiner si besoin).
    "helipads": [
        ("Aéroport de Bordeaux-Mérignac", -0.6964, 44.8362),
        ("CHU Pellegrin", -0.6045, 44.8378),
        ("Hôpital Haut-Lévêque (Pessac)", -0.6330, 44.7908),
        ("Hôpital Saint-André", -0.5790, 44.8333),
    ],
    # Balise HAPI (indicateur de pente d'approche pour hélicoptère, voir
    # media/gt_installation_hapi.pdf) sur le pad de départ. Azimut = cap de
    # départ (90°, valeur par défaut : pas de start_heading propre à cette
    # zone) ; pente 6 % : valeur usuelle pour une hélistation.
    "hapi": [
        ("Aéroport de Bordeaux-Mérignac", -0.6964, 44.8362, 90, 6),
    ],
}
