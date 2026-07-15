# -*- coding: utf-8 -*-
"""
cote-landes
Côte basco-landaise, de Bayonne / Anglet (embouchure de l'Adour) au sud
jusqu'à Vieux-Boucau-les-Bains au nord. Bord de mer : l'océan à l'ouest est
hors couverture BD ORTHO (blanc) et se fait aplanir en mer unie.
"""

ZONE = {
    "bbox": (-1.62, -1.30, 43.46, 43.81),
    "recolor_sea": True,
    # Calé sur l'hélipad de Capbreton (voir "helipads" ci-dessous) : l'ancien
    # indice "arrière-plage plate vers Hossegor / Capbreton" tombait au ras de
    # l'eau (0 m d'altitude, bord du lac d'Hossegor), et le calage "hélipad le
    # plus proche" (ApplicationScene.cpp) devenait instable d'une régénération
    # à l'autre. On vise donc directement les coordonnées exactes du pad.
    "start": (-1.4457112839188175, 43.65393627677582),  # Capbreton
    "start_heading": 0,       # face au nord : la côte file vers Vieux-Boucau
    "grid": 1024,      # maille ~25-38 m au lieu de ~50-76 m (relief plus lisse)
    "ortho_px": 5000,  # ortho ~7,8 m/px au lieu de ~19 (limite serveur IGN : 5010)
    "title": "côte basco-landaise (Bayonne -> Vieux-Boucau)",
    "landmarks": [
        ("Bayonne", -1.4750, 43.4933),
        ("Anglet", -1.5150, 43.4850),
        ("Boucau", -1.4711, 43.5269),
        ("Tarnos", -1.4606, 43.5408),
        ("Ondres", -1.4510, 43.5650),
        ("Labenne", -1.4347, 43.5917),
        ("Capbreton", -1.4310, 43.6420),
        ("Hossegor", -1.3950, 43.6640),
        ("Seignosse", -1.3780, 43.6890),
        ("Vieux-Boucau", -1.4010, 43.7880),
        # Lieux ajoutés (coordonnées IGN, îles et lacs recoupés OpenStreetMap).
        ("Biarritz", -1.5639, 43.4795),
        ("Rocher de la Vierge", -1.5703, 43.4841),
        ("Aéroport de Biarritz", -1.5233, 43.4687),
        ("Soustons", -1.3413, 43.7699),
        ("Lac d'Hossegor", -1.4287, 43.6722),
        ("Étang de Soustons", -1.3292, 43.7703),
        ("Étang Blanc", -1.3653, 43.7094),
        ("St-Vincent-de-Tyrosse", -1.3055, 43.6667),
        ("Bénesse-Maremne", -1.3695, 43.6338),
        ("Marais d'Orx", -1.3972, 43.6007),
        ("Saint-Martin-de-Seignanx", -1.3952, 43.5354),
    ],
    # Coordonnées relevées sur Google Maps.
    "helipads": [
        ("Hôpital de Bayonne", -1.4800367078111412, 43.48262235303451),
        ("Labenne plage", -1.4726675619926326, 43.599308117206505),
        ("Capbreton", -1.4457112839188175, 43.65393627677582),
        ("Hossegor", -1.4438385382046726, 43.661316497891036),
    ],
    # Estacade de Capbreton (jetée sud du chenal du Boucarot, où sont fixés les
    # deux feux d'entrée du port, l'un vert au bout de l'estacade sud, l'autre
    # rouge au bout de la digue nord, cf. instructions nautiques). Un arbre y
    # poussait par erreur (bande de sable/eau mal classée par le scatter de
    # végétation) : exclusion nécessaire, comme pour les pistes d'aérodrome.
    "exclusions": [
        ("Estacade de Capbreton", -1.4488, 43.6552, 200),
    ],
    # Balise HAPI (indicateur de pente d'approche pour hélicoptère, voir
    # media/gt_installation_hapi.pdf) sur le pad de départ. Azimut = cap de
    # départ (aucune piste réelle recensée ici, contrairement à Dax) ; pente
    # 6 % : valeur usuelle pour une hélistation.
    "hapi": [
        ("Capbreton", -1.4457112839188175, 43.65393627677582, 0, 6),
    ],
}
