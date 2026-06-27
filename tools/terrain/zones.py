# -*- coding: utf-8 -*-
"""
zones.py
Description des terrains réels disponibles. Chaque zone décrit une emprise
géographique (WGS84), son comportement vis-à-vis de la mer (recolor_sea), son
point de départ du vol et ses lieux remarquables. Pour ajouter une zone : copier
une entrée et changer les bornes. La sortie va dans assets/terrain/<nom>/.

  bbox        : (lon_min, lon_max, lat_min, lat_max)
  recolor_sea : True en bord de mer (mer aplanie), False en montagne (sans mer).
                Pilote aussi le plan de mer du moteur (clé "sea" du calage).
  start       : (lon, lat) du point de départ ; le script affine sur le replat
                le plus proche (voir find_flat_start).
  title       : libellé écrit en commentaire dans terrain.txt.
  landmarks   : liste de (nom, lon, lat) étiquetés sur la scène et la minimap.
  helipads    : liste de (nom, lon, lat) où poser un hélipad (hôpital, port...).
                Facultatif ; l'hélipad de départ du vol est ajouté à part.

Auteur : O. Booklage
Licence : GPL v2
"""

ZONES = {
    # Vallée d'Ossau (lac d'Artouste, pic du Midi d'Ossau, ~2884 m) : le lieu-titre
    # du projet (la turbine Artouste de l'Alouette II est nommée d'après ce massif).
    # Montagne sans mer.
    "ossau": {
        "bbox": (-0.52, -0.30, 42.80, 42.96),
        "recolor_sea": False,
        "start": (-0.413, 42.905),  # Fabrèges, fond de vallée plat (~1230 m)
        # Montagne : on garde les petites constructions (cabanes, bergeries, granges),
        # nombreuses en estive et utiles au repérage, donc seuil de hauteur à 0.
        "height_min": 0.0,
        "title": "vallée d'Ossau (lac d'Artouste, pic du Midi d'Ossau)",
        "landmarks": [
            ("Lac d'Artouste", -0.3325, 42.8589),
            ("Pic du Midi d'Ossau", -0.4380, 42.8430),
            ("Pic Palas", -0.3600, 42.8400),
            ("Fabrèges", -0.4130, 42.9050),
        ],
    },
    # Côte basco-landaise, de Bayonne / Anglet (embouchure de l'Adour) au sud
    # jusqu'à Vieux-Boucau-les-Bains au nord. Bord de mer : l'océan à l'ouest est
    # hors couverture BD ORTHO (blanc) et se fait aplanir en mer unie.
    "cote-landes": {
        "bbox": (-1.62, -1.30, 43.46, 43.81),
        "recolor_sea": True,
        "start": (-1.43, 43.66),  # arrière-plage plate vers Hossegor / Capbreton
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
        ],
        # Coordonnées relevées sur Google Maps.
        "helipads": [
            ("Hôpital de Bayonne", -1.4800367078111412, 43.48262235303451),
            ("Labenne plage", -1.4726675619926326, 43.599308117206505),
            ("Capbreton", -1.4457112839188175, 43.65393627677582),
            ("Hossegor", -1.4438385382046726, 43.661316497891036),
        ],
    },
    # Bassin d'Arcachon et son arrière-pays : de la côte atlantique et du Cap Ferret
    # à l'ouest jusqu'à Marcheprime à l'est (vers Bordeaux), et de Lège / Arès au nord
    # jusqu'à Biscarrosse et ses étangs au sud. Bord de mer : l'océan, le bassin et
    # les grands étangs (Cazaux-Sanguinet) sont aplanis en eau unie.
    "arcachon": {
        "bbox": (-1.28, -0.83, 44.36, 44.80),
        "recolor_sea": True,
        "start": (-1.111, 44.596),  # aérodrome de La Teste, replat boisé loin de l'eau
        "title": "bassin d'Arcachon (Cap Ferret, Biscarrosse, Marcheprime)",
        "landmarks": [
            ("Arcachon", -1.1683, 44.6586),
            ("Cap Ferret", -1.2486, 44.6336),
            ("Dune du Pilat", -1.2114, 44.5886),
            ("La Teste-de-Buch", -1.1450, 44.6314),
            ("Gujan-Mestras", -1.0697, 44.6361),
            ("Biganos", -0.9744, 44.6453),
            ("Audenge", -1.0186, 44.6839),
            ("Andernos-les-Bains", -1.1044, 44.7436),
            ("Arès", -1.1392, 44.7686),
            ("Marcheprime", -0.8506, 44.6953),
            ("Biscarrosse", -1.1664, 44.3936),
            ("Biscarrosse-Plage", -1.2461, 44.4564),
        ],
        # Coordonnées relevées sur Google Maps.
        "helipads": [
            ("Aérodrome de La Teste", -1.116442178115823, 44.596643236436016),
        ],
    },
    # Cauterets - Gavarnie (Hautes-Pyrénées) : de la vallée de Cauterets et son
    # chemin des cascades (jusqu'au Pont d'Espagne et au lac de Gaube, sous le
    # Vignemale, 3298 m) au sud-est jusqu'au cirque de Gavarnie (classé UNESCO)
    # et la brèche de Roland. Haute montagne sans mer ; comme pour Ossau on garde
    # les petites constructions (refuges, granges) utiles au repérage.
    "cauterets": {
        "bbox": (-0.16, 0.03, 42.68, 42.90),
        "recolor_sea": False,
        "start": (-0.110, 42.886),  # fond de vallée de Cauterets (~930 m), plat
        "height_min": 0.0,
        "title": "Cauterets - Gavarnie (chemin des cascades, Pont d'Espagne, cirque de Gavarnie)",
        "landmarks": [
            ("Cauterets", -0.1124, 42.8903),
            ("Chemin des cascades", -0.1246, 42.8601),  # cascade du Pas de l'Ours
            ("Pont d'Espagne", -0.1437, 42.8556),
            ("Lac de Gaube", -0.1465, 42.8400),
            ("Vignemale", -0.1456, 42.7700),
            ("Luz-Saint-Sauveur", 0.0000, 42.8730),
            ("Gèdre", 0.0175, 42.7847),
            ("Gavarnie", -0.0086, 42.7335),
            ("Brèche de Roland", -0.0386, 42.6975),
            ("Cirque de Gavarnie", -0.0090, 42.6963),
        ],
        # Hélistations : bases de secours en vallée (PGHM Cauterets, CRS de Gavarnie,
        # Luz, parking du Pont d'Espagne) et deux DZ de refuge d'altitude, fidèles au
        # rôle de l'Alouette II en montagne (secours et ravitaillement des refuges).
        "helipads": [
            ("Cauterets", -0.1120, 42.8880),
            ("Pont d'Espagne", -0.1440, 42.8560),
            ("Gavarnie", -0.0090, 42.7330),
            ("Luz-Saint-Sauveur", 0.0000, 42.8730),
            ("Refuge des Oulettes de Gaube", -0.1412, 42.7929),
            ("Refuge des Sarradets", -0.0333, 42.6959),
        ],
    },
    # Bordeaux et son agglomération : la Garonne et le port de la Lune au centre,
    # de l'aéroport de Mérignac et Pessac à l'ouest jusqu'à Cenon et Lormont sur la
    # rive droite, et de Villenave-d'Ornon au sud jusqu'au stade Matmut Atlantique et
    # Bruges au nord. Terre intérieure : pas d'océan, la Garonne garde sa vraie
    # imagerie (pas de recoloration de mer).
    "bordeaux": {
        "bbox": (-0.74, -0.42, 44.72, 44.96),
        "recolor_sea": False,
        "start": (-0.7155, 44.8286),  # aéroport de Bordeaux-Mérignac, vaste replat
        # Ville très dense : on relève le seuil à 5 m pour écarter garages, abris et
        # petites annexes, et garder un buildings.bin raisonnable (le détail à 2 m
        # pesait près de 30 Mo).
        "height_min": 5.0,
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
            ("Aéroport de Mérignac", -0.7155, 44.8286),
        ],
        # Aéroport et hôpitaux dotés d'une aire de poser (coordonnées relevées sur
        # la carte ; à affiner si besoin).
        "helipads": [
            ("Aéroport de Bordeaux-Mérignac", -0.7155, 44.8286),
            ("CHU Pellegrin", -0.6045, 44.8378),
            ("Hôpital Haut-Lévêque (Pessac)", -0.6330, 44.7908),
            ("Hôpital Saint-André", -0.5790, 44.8333),
        ],
    },
}

DEFAULT_ZONE = "ossau"
