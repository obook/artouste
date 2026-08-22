"""
toulouse
Toulouse et l'aérodrome de Lasbordes (LFCL), à l'est de la ville. Emprise
compacte (~12x12 km) qui tient le terrain d'aviation, la Cité de l'espace, la
Garonne, le centre-ville (Capitole, Saint-Sernin, les Jacobins, Pont Neuf), le
canal du Midi et l'ancienne piste de Montaudran. Terre intérieure, terrain
plat (~130 à 190 m) : pas de mer.

Le départ est posé sur l'aire de poser de Lasbordes, au nord du parking des
aéro-clubs, le long de la piste 15/33.
"""

ZONE = {
    "bbox": (1.395, 1.545, 43.535, 43.645),
    "recolor_sea": False,
    # Aire de poser relevée sur l'orthophoto par l'utilisateur, au nord du
    # parking des aéro-clubs, près de la manche à air.
    "start": (1.500530, 43.586813),
    "start_heading": 300,  # face à l'ouest-nord-ouest : la piste, puis le Capitole
    # À ajuster visuellement : petites annexes du bâti pavillonnaire.
    "height_min": 3.0,
    "grid": 1024,       # mailles ~11,9 m
    "ortho_px": 6000,   # ~2,0 m/px ; mosaïque WMS 2x2 (limite serveur IGN : 5010/tuile)
    "title": "Toulouse (aérodrome de Lasbordes, Capitole, Garonne, Cité de l'espace)",
    # Coordonnées OpenStreetMap (requête Overpass sur l'emprise).
    "landmarks": [
        ("Toulouse", 1.4442, 43.6045),
        ("Le Capitole", 1.4443, 43.6044),
        ("Basilique Saint-Sernin", 1.4419, 43.6084),
        ("Cathédrale Saint-Étienne", 1.4504, 43.5999),
        ("Église des Jacobins", 1.4402, 43.6035),
        ("Basilique Notre-Dame de la Daurade", 1.4399, 43.6009),
        ("Pont Neuf", 1.4391, 43.5993),
        ("Hôtel-Dieu Saint-Jacques", 1.4365, 43.5998),
        ("Musée des Augustins", 1.4463, 43.6010),
        ("Muséum d'Histoire naturelle", 1.4496, 43.5934),
        ("Jardin des Plantes", 1.4508, 43.5930),
        ("La Halle aux Grains", 1.4545, 43.5999),
        ("Prairie des Filtres", 1.4366, 43.5956),
        ("Île du Ramier", 1.4336, 43.5832),
        ("Stadium de Toulouse", 1.4340, 43.5832),
        ("Palais des Sports", 1.4354, 43.6122),
        ("Gare de Toulouse-Matabiau", 1.4542, 43.6114),
        ("Pont de l'Embouchure", 1.4119, 43.6090),
        ("Pont des Catalans", 1.4280, 43.6032),
        # Est : l'aérodrome et la mémoire aéronautique de Montaudran.
        ("Aérodrome de Toulouse-Lasbordes", 1.4998, 43.5872),
        ("Cité de l'espace", 1.4930, 43.5860),
        ("Halle de La Machine", 1.4781, 43.5728),
        ("L'Envol des Pionniers", 1.4775, 43.5741),
        ("Piste de Montaudran", 1.4886, 43.5736),
        ("Le Castelet", 1.4475, 43.5861),
        # Quartiers et communes : couverture des bords de la carte.
        ("Saint-Cyprien", 1.4306, 43.5979),
        ("Croix de Pierre", 1.4274, 43.5846),
        ("Empalot", 1.4397, 43.5799),
        ("Rangueil", 1.4591, 43.5740),
        ("Côte Pavée", 1.4625, 43.5959),
        ("Jolimont", 1.4630, 43.6143),
        ("Soupetard", 1.4782, 43.6105),
        ("Les Minimes", 1.4367, 43.6183),
        ("Sept Deniers", 1.4109, 43.6144),
        ("Stade Ernest-Wallon", 1.4154, 43.6218),
        ("Ginestous", 1.4109, 43.6387),
        ("Borderouge", 1.4527, 43.6387),
        ("Croix Daurade", 1.4640, 43.6356),
        ("Balma", 1.4980, 43.6097),
        ("Pin-Balma", 1.5337, 43.6283),
        ("Montrabé", 1.5252, 43.6428),
        ("Quint-Fonsegrives", 1.5283, 43.5855),
        ("Saint-Orens-de-Gameville", 1.5340, 43.5520),
        ("Ramonville-Saint-Agne", 1.4749, 43.5461),
        ("Pouvourville", 1.4544, 43.5480),
        ("Hôpital Rangueil", 1.4521, 43.5604),
        ("Hôpital Larrey", 1.4537, 43.5526),
        ("Lafourguette", 1.4108, 43.5649),
        ("Hôpital Purpan", 1.4013, 43.6098),
    ],
    # Aires de poser réelles, les trois relevées sur l'orthophoto IGN. Le pad de
    # départ sert aussi d'indice au moteur, qui retient l'hélipad le plus proche.
    "helipads": [
        ("Aérodrome de Toulouse-Lasbordes", 1.500530, 43.586813),
        ("CHU Purpan", 1.400065, 43.613242),
        ("CHU Rangueil", 1.448630, 43.561258),
    ],
    # Piste et abords de l'aérodrome : herbe verte dans l'ortho, sinon boisée
    # par erreur (comme La Teste à Arcachon et Seyresse à Dax).
    "exclusions": [
        ("Aérodrome de Toulouse-Lasbordes (LFCL)", 1.4995, 43.5862, 650),
    ],
    # Balise HAPI (indicateur de pente d'approche pour hélicoptère, voir
    # media/gt_installation_hapi.pdf). Azimut 330 : aligné sur la piste bitumée
    # 15/33, approche vers le nord-ouest (QFU 33). Pente 6 % : valeur usuelle
    # pour une hélistation, faute de relevé d'obstacles réel.
    "hapi": [
        ("Aérodrome de Toulouse-Lasbordes", 1.500530, 43.586813, 330, 6),
    ],
}
