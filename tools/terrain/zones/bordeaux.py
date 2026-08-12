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
        ("Mérignac", -0.6570, 44.8451),  # mairie
        ("Pessac", -0.6311, 44.8067),
        ("Talence", -0.5912, 44.8080),
        ("Bègles", -0.5436, 44.8082),  # mairie
        ("Cenon", -0.5300, 44.8560),
        ("Lormont", -0.5231, 44.8800),  # mairie
        ("Le Bouscat", -0.5972, 44.8650),
        ("Bruges", -0.6127, 44.8815),  # mairie
        ("Stade Matmut Atlantique", -0.5614, 44.8956),
        ("Aéroport de Mérignac", -0.7028, 44.8309),  # aérogare
        # Lieux ajoutés (coordonnées géocodeur IGN).
        ("Cathédrale Saint-André", -0.5779, 44.8376),
        ("Gare Saint-Jean", -0.5564, 44.8262),
        ("Pont Chaban-Delmas", -0.5517, 44.8581),
        ("Pont d'Aquitaine", -0.5390, 44.8803),
        ("Gradignan", -0.6083, 44.7725),  # mairie
        ("Floirac", -0.5276, 44.8305),  # mairie
        ("Villenave-d'Ornon", -0.5717, 44.7829),  # mairie
        # Couronne de la carte, restée sans repère : bourgs relevés sur
        # OpenStreetMap (nœuds place=town/village).
        ("Saint-Médard-en-Jalles", -0.7171, 44.8959),
        ("Le Taillan-Médoc", -0.6693, 44.9044),
        ("Saint-Aubin-de-Médoc", -0.7247, 44.9134),
        ("Ambarès-et-Lagrave", -0.4882, 44.9259),
        ("Sainte-Eulalie", -0.4743, 44.9062),
        ("Artigues-près-Bordeaux", -0.4935, 44.8603),
        ("Tresses", -0.4636, 44.8494),
        ("Yvrac", -0.4618, 44.8808),
        ("Bouliac", -0.5037, 44.8140),
        ("Carignan-de-Bordeaux", -0.4747, 44.8129),
        ("Cestas", -0.6841, 44.7412),
        ("Canéjan", -0.6541, 44.7627),
        ("Latresne", -0.4969, 44.7846),
        ("Cénac", -0.4618, 44.7800),
        ("Quinsac", -0.4891, 44.7557),
        ("Cambes", -0.4629, 44.7320),
        ("Isle-Saint-Georges", -0.4735, 44.7264),
    ],
    # Aéroport et hôpitaux dotés d'une aire de poser (coordonnées relevées sur
    # la carte ; à affiner si besoin).
    "helipads": [
        ("Aéroport de Bordeaux-Mérignac", -0.6964, 44.8362),
        ("CHU Pellegrin", -0.6039, 44.8275),
        ("Hôpital Haut-Lévêque (Pessac)", -0.6609, 44.7853),
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
