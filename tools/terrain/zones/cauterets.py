"""
cauterets
Cauterets - Gavarnie (Hautes-Pyrénées) : de la vallée de Cauterets et son
chemin des cascades (jusqu'au Pont d'Espagne et au lac de Gaube, sous le
Vignemale, 3298 m) au sud-est jusqu'au cirque de Gavarnie (classé UNESCO)
et la brèche de Roland. Haute montagne sans mer ; comme pour Ossau on garde
les petites constructions (refuges, granges) utiles au repérage.
"""

ZONE = {
    "bbox": (-0.16, 0.03, 42.68, 42.90),
    "recolor_sea": False,
    "start": (-0.110, 42.886),  # fond de vallée de Cauterets (~930 m), plat
    "start_heading": 180,       # face au sud : vers le fond de la vallée
    "height_min": 0.0,
    "grid": 1024,      # montagne : relief ~24 m au lieu de ~48 (comme ossau)
    "ortho_px": 5000,  # ortho ~4,9 m/px au lieu de ~12 (limite serveur IGN : 5010)
    "title": "Cauterets - Gavarnie (chemin des cascades, Pont d'Espagne, cirque de Gavarnie)",
    "landmarks": [
        ("Cauterets", -0.1124, 42.8903),
        ("Chemin des cascades", -0.1246, 42.8601),  # cascade du Pas de l'Ours
        ("Pont d'Espagne", -0.1398, 42.8509),  # site réel (confluence, parking, ~1496 m)
        ("Lac de Gaube", -0.139722, 42.831138),
        ("Vignemale", -0.1472, 42.77389),
        ("Luz-Saint-Sauveur", -0.0033, 42.8715),  # église fortifiée Saint-André
        ("Gèdre", 0.0175, 42.7847),
        ("Gavarnie", -0.0086, 42.7335),
        ("Brèche de Roland", -0.03361, 42.69083),
        ("Cirque de Gavarnie", -0.0090, 42.6963),
        # Lieux ajoutés (coordonnées recoupées IGN + OpenStreetMap).
        ("Lac d'Estom", -0.1001, 42.8063),
        ("Piméné", 0.0216, 42.7356),
        ("Pic du Marboré", 0.0119, 42.6925),
        # Lieux ajoutés (coordonnées OpenStreetMap).
        ("Esterre", 0.0061, 42.8749),
        ("Esquièze-Sère", -0.0025, 42.8756),
        ("Sazos", -0.0248, 42.8838),
        ("Grust", -0.0318, 42.8891),
        ("Grande Cascade de Gavarnie", -0.0045, 42.6933),
        ("Pont Napoléon", -0.0056, 42.8584),
        ("Thermes de Saint-Sauveur", -0.0111, 42.8637),
        ("Cascade de Lutour", -0.1064, 42.8709),
        ("Cascade du Cerisey", -0.1189, 42.8632),
        ("Refuge de Bayssellance", -0.1241, 42.7794),
        # Massif d'Ardiden et frontière espagnole, restés sans repère (sommets
        # et cols relevés sur OpenStreetMap, altitude en commentaire).
        ("Luz-Ardiden", -0.0588, 42.8851),  # station de ski
        ("Col de Riou", -0.0708, 42.8963),  # 1950 m
        ("Pic Né", -0.0720, 42.8595),  # 2665 m
        ("Pic de Cestrède", -0.0706, 42.8036),  # 2947 m
        ("Lac de Cestrède", -0.0383, 42.8056),
        ("Pic de Barbe de Bouc", -0.0544, 42.8220),  # 2964 m
        ("Col de la Bernatoire", -0.1004, 42.7223),  # 2338 m
        ("Pic Crabère", -0.1119, 42.7266),  # 2519 m
    ],
    # Hélistations : bases de secours en vallée (PGHM Cauterets, CRS de Gavarnie,
    # Luz, parking du Pont d'Espagne) et trois DZ de refuge d'altitude, fidèles au
    # rôle de l'Alouette II en montagne (secours et ravitaillement des refuges).
    "helipads": [
        ("Cauterets", -0.1120, 42.8880),
        ("Pont d'Espagne", -0.1398, 42.8509),
        ("Gavarnie", -0.0090, 42.7330),
        ("Luz-Saint-Sauveur", -0.0033, 42.8715),
        ("Refuge des Oulettes de Gaube", -0.1412, 42.7929),
        ("Refuge des Sarradets", -0.0333, 42.6959),
        ("Refuge de Bayssellance", -0.1241, 42.7794),
    ],
    # Balise HAPI (indicateur de pente d'approche pour hélicoptère, voir
    # media/gt_installation_hapi.pdf) sur le pad de départ. Azimut = cap de
    # départ (aucune piste réelle recensée ici, contrairement à Dax) ; pente
    # 6 % : valeur usuelle pour une hélistation.
    "hapi": [
        ("Cauterets", -0.1120, 42.8880, 180, 6),
    ],
}
