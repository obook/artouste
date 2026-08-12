"""
pau
Pau et l'aéroport de Pau-Pyrénées à Uzein (LFBP), qui abrite la base de
l'Aviation légère de l'Armée de terre. Plaine du Gave de Pau et coteaux du
Béarn : terrain vallonné sans relief marqué, pas de mer.

L'emprise n'est PAS centrée sur le pad de la base : celui-ci est à 9,9 km du
centre de Pau, si bien qu'un carré de 18 km posé sur lui laisserait le château
et le boulevard des Pyrénées 400 m hors cadre. Le centre est donc pris à
mi-chemin des deux (43,3370 / -0,3912), ce qui tient l'aéroport, la base, toute
la ville et le Gave, avec encore 8 km de marge au nord du pad. Le départ, lui,
reste sur le pad : rien n'oblige le point de départ à être au centre de la
carte (voir "start" ci-dessous et le cas de Dax).

Format 18 x 18 km, celui d'Ossau et de Bigorre. Le dépôt compte déjà trop de
tailles différentes (2, 6, 15,5, 16, 18, 25 et 36 km de côté) : on s'en tient
désormais à 6 km pour une ville et son aérodrome, 18 km pour une région.
"""

ZONE = {
    "bbox": (-0.5024, -0.2801, 43.2561, 43.4178),
    "recolor_sea": False,
    # Pad de la base ALAT de Pau-Uzein. Position affinée par l'utilisateur le
    # 08/08/2026, à l'est de la précédente : l'imagerie y est nette, alors que
    # l'ouest de la plateforme est flouté (voir la note sur le 5e RHC plus bas).
    "start": (-0.407849, 43.378080),
    # Nez au sud : vers Pau, le Gave et la ligne des Pyrénées au-delà.
    "start_heading": 180,
    # Mailles de ~17,6 m sur 18 km, comme Bigorre. Le Béarn est vallonné, pas
    # montagneux : la grille de 2048 d'Ossau n'y apporterait pas grand-chose.
    "grid": 1024,
    # ~1,8 m/px, mosaïque WMS 2x2 (limite serveur IGN : 5010 px par tuile).
    # Plus fin que les cartes de montagne, et il y a une raison : la couleur des
    # toitures est lue dans l'orthophoto, et n'est retenue qu'à partir de deux
    # pixels par emprise de bâtiment (voir BuildingsMesh.cpp). À 1,8 m/px, tout
    # bâtiment de plus de 3,6 m de côté prend donc sa vraie teinte.
    "ortho_px": 10000,
    "title": "Pau (aéroport de Pau-Pyrénées et base ALAT d'Uzein, Gave de Pau)",
    # Positions relevées sur OpenStreetMap le 08/08/2026 : centre de l'emprise
    # pour un bâtiment ou un équipement, noeud de la commune pour un village.
    "landmarks": [
        ("Pau", -0.36857, 43.29575),
        ("Château de Pau", -0.37558, 43.29480),
        ("Gare de Pau", -0.36975, 43.29147),
        ("Musée des Beaux-Arts", -0.36438, 43.29774),
        ("Parc Beaumont", -0.36047, 43.29723),
        ("Aéroport de Pau-Pyrénées", -0.42020, 43.37914),
        ("Base ALAT de Pau-Uzein", -0.41909, 43.37156),
        ("Chapelle Mémorial de l'Aviation", -0.42011, 43.35517),
        ("Musée des Parachutistes", -0.38709, 43.34618),
        ("Camp Aspirant André Zirnheld", -0.39314, 43.34761),
        ("Hippodrome du Pont-Long", -0.37300, 43.33622),
        ("Stade du Hameau", -0.31690, 43.30953),
        ("Zénith Pyrénées", -0.36491, 43.33560),
        ("Centre hospitalier de Pau", -0.34717, 43.32615),
        ("Haras National de Gelos", -0.36785, 43.28381),
        ("Lescar", -0.43573, 43.33338),
        ("Lons", -0.40976, 43.31541),
        ("Billère", -0.39056, 43.30300),
        ("Jurançon", -0.38964, 43.28718),
        ("Gelos", -0.37095, 43.28519),
        ("Bizanos", -0.35314, 43.28880),
        ("Idron", -0.31175, 43.29108),
        ("Uzein", -0.43309, 43.39904),
        ("Serres-Castet", -0.35420, 43.38619),
        ("Montardon", -0.35027, 43.36828),
        ("Sauvagnon", -0.38589, 43.40440),
        ("Mazères-Lezons", -0.35647, 43.27855),
        ("Poey-de-Lescar", -0.47005, 43.35099),
        ("Artiguelouve", -0.47267, 43.32014),
        ("Laroin", -0.44268, 43.30570),
    ],
    # Aires de poser relevées sur OpenStreetMap (quatorze nœuds "helipad" dans
    # l'emprise) puis triées sur l'orthophoto à 25 cm : la douzaine groupée sur la
    # base sont ses postes de stationnement, pas des aires de poser distinctes,
    # même remarque qu'à Dax. Restent trois aires utiles. Une quatrième, à
    # -0.29712 / 43.29893 près de Lée, n'a pas de nom dans OSM : non retenue tant
    # qu'on ne sait pas ce qu'elle dessert.
    "helipads": [
        ("Base ALAT de Pau-Uzein", -0.407849, 43.378080),
        # Base de la Sécurité civile au nord de l'aéroport : aire bétonnée à
        # cercle jaune, avec l'appareil dessus sur la photo.
        ("Dragon 64 (Sécurité civile)", -0.417500, 43.383530),
        # Plateforme sur remblai, 1 626 m2 mesurés sur l'ortho, marquage en
        # losange. Position et cap du H (135) arrêtés par l'utilisateur.
        ("Centre hospitalier de Pau", -0.350714, 43.327953),
        # Hélistation officielle (IGN BD TOPO, couche aerodrome), à 327 m du pad
        # de la base : plateforme bétonnée isolée en herbe le long du taxiway,
        # distincte des postes de stationnement écartés plus haut.
        ("Hélistation de Pau-Pyrénées", -0.411750, 43.378880),
    ],
    # Balise HAPI sur le pad de départ. Azimut 125 : l'axe de la piste 13/31,
    # mesuré sur l'orthophoto (2 532 m entre seuils, 125/305 degrés). Pente 6 %,
    # valeur usuelle d'hélistation, faute de relevé d'obstacles réel.
    "hapi": [
        ("Base ALAT de Pau-Uzein", -0.407849, 43.378080, 125, 6),  # HAPI : axe de piste
    ],
    # Plateforme de Pau-Pyrénées : herbe rase le long de la piste, que le semis
    # de végétation prendrait pour une prairie et couvrirait d'arbres (constaté
    # en vol). Cinq cercles le long de l'axe plutôt qu'un seul : la piste fait
    # 2 532 m, un cercle unique assez grand pour la couvrir raserait aussi les
    # bois alentour. Même remède qu'à Dax et qu'à La Teste.
    "exclusions": [
        ("Aéroport de Pau-Pyrénées (LFBP) 1/5", -0.4332, 43.3877, 450),
        ("Aéroport de Pau-Pyrénées (LFBP) 2/5", -0.4268, 43.3844, 450),
        ("Aéroport de Pau-Pyrénées (LFBP) 3/5", -0.4204, 43.3811, 450),
        ("Aéroport de Pau-Pyrénées (LFBP) 4/5", -0.4140, 43.3779, 450),
        ("Aéroport de Pau-Pyrénées (LFBP) 5/5", -0.4076, 43.3746, 450),
    ],
}

# NOTE SUR L'IMAGERIE. Le sud-ouest de la plateforme, emprise du 5e régiment
# d'hélicoptères de combat, est FLOUTÉ à la source : l'IGN y sert des pavés de
# douze mètres, et Google Maps fait de même. Vérifié le 08/08/2026 par sondage
# du serveur WMS à 25 cm (69 % de blocs plats à 500 m à l'ouest du pad, contre
# 2 % sur le pad lui-même). L'orthophoto d'ensemble à 1,80 m/pixel le dissimule
# presque ; les tuiles de détail à 0,25 m le montrent en patchwork. Rien à
# corriger côté téléchargement, la donnée source est ainsi.
