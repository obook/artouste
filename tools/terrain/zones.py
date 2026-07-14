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
  grid        : largeur et hauteur de la grille d'altitude en nombre de mailles
                (défaut 512).
  ortho_px    : hauteur en pixels de l'orthophoto téléchargée (défaut 2048).
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
        "start": (-0.3972, 42.8799),  # hameau de Fabrèges, au bord du lac (~1250 m)
        "start_heading": 180,       # face au sud, vers le lac et le pic du Midi d'Ossau
        # find_flat_start peut dériver sur le lac voisin (parfaitement plat) :
        # start_x/start_z de terrain.txt ont été calés à la main sur la terre ferme.
        # Montagne : on garde les petites constructions (cabanes, bergeries, granges),
        # nombreuses en estive et utiles au repérage, donc seuil de hauteur à 0.
        "height_min": 0.0,
        "grid": 1024,      # mailles ~17,5 m (défaut 512 pour les autres zones)
        "ortho_px": 4096,  # hauteur d'ortho ~4,4 m/px (défaut 2048)
        "title": "vallée d'Ossau (lac d'Artouste, pic du Midi d'Ossau)",
        "landmarks": [
            ("Lac d'Artouste", -0.3325, 42.8589),
            ("Pic du Midi d'Ossau", -0.4380, 42.8430),
            ("Pic Palas", -0.3600, 42.8400),
            ("Fabrèges", -0.3972, 42.8799),
            # Lieux ajoutés (coordonnées géocodeur IGN, data.geopf.fr).
            ("Lac de Fabrèges", -0.3974, 42.8767),
            ("Gabas", -0.4273, 42.8889),
            ("Lac de Bious-Artigues", -0.4537, 42.8664),
            ("Pic de la Sagette", -0.4037, 42.8972),
            ("Lac de Pombie", -0.4282, 42.8358),
            ("Pic Peyreget", -0.4414, 42.8309),
            ("Col du Pourtalet", -0.4180, 42.8049),
            ("Pic d'Artouste", -0.3101, 42.8549),
            ("Lac de Bersau", -0.4949, 42.8395),
            # Lieux ajoutés (coordonnées OpenStreetMap).
            ("Barrage d'Artouste", -0.3327, 42.8628),
            ("Petit train d'Artouste", -0.3428, 42.8758),
            ("Refuge de Pombie", -0.4269, 42.8355),
            ("Refuge d'Ayous", -0.4912, 42.8484),
            ("Refuge d'Arrémoulit", -0.3292, 42.8460),
            ("Lac d'Ayous", -0.4792, 42.8481),
            ("Col d'Ayous", -0.4971, 42.8519),
            ("Col de Suzon", -0.4234, 42.8470),
            ("Lac de Peyreget", -0.4483, 42.8343),
            ("Pic de Sesques", -0.5038, 42.9192),
        ],
        # Refuges de montagne (bases de secours du PGHM, fidèles au rôle de
        # l'Alouette II gendarmerie) et barrage d'Artouste, en plus du pad de
        # départ et du sommet. Coordonnées OpenStreetMap.
        "helipads": [
            # Fabrèges : hameau au bord du lac, aire de départ du vol.
            ("Fabrèges", -0.3972, 42.8799),
            # Sommet du pic du Midi d'Ossau : point culminant de la heightmap 1024
            # (~2847 m). Centrer sur le maximum local évite d'enterrer le disque
            # côté amont ; la plate-forme du moteur (heightAt + jupe) fait le reste.
            ("Pic du Midi d'Ossau", -0.4382796, 42.8430108),
            ("Refuge de Pombie", -0.4269, 42.8355),
            ("Refuge d'Ayous", -0.4912, 42.8484),
            ("Refuge d'Arrémoulit", -0.3292, 42.8460),
            ("Barrage d'Artouste", -0.3327, 42.8628),
        ],
    },
    # Côte basco-landaise, de Bayonne / Anglet (embouchure de l'Adour) au sud
    # jusqu'à Vieux-Boucau-les-Bains au nord. Bord de mer : l'océan à l'ouest est
    # hors couverture BD ORTHO (blanc) et se fait aplanir en mer unie.
    "cote-landes": {
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
    },
    # Bassin d'Arcachon et son arrière-pays : de la côte atlantique et du Cap Ferret
    # à l'ouest jusqu'à Marcheprime à l'est (vers Bordeaux), et de Lège / Arès au nord
    # jusqu'à Biscarrosse et ses étangs au sud. Bord de mer : l'océan, le bassin et
    # les grands étangs (Cazaux-Sanguinet) sont aplanis en eau unie.
    "arcachon": {
        "bbox": (-1.28, -0.83, 44.36, 44.80),
        "recolor_sea": True,
        "start": (-1.111, 44.596),  # aérodrome de La Teste, replat boisé loin de l'eau
        "start_heading": 330,       # N30°O : face à Arcachon et au bassin
        "grid": 1024,      # mailles ~34,9 m au lieu de ~69,7 m (défaut 512)
        "ortho_px": 5000,  # ortho ~9,8 m/px au lieu de ~24 (limite serveur IGN : 5010)
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
            # Lieux ajoutés (coordonnées IGN, île et lac recoupés OpenStreetMap).
            ("Île aux Oiseaux", -1.1780, 44.6985),
            ("Banc d'Arguin", -1.2430, 44.5855),
            ("Phare du Cap Ferret", -1.2488, 44.6460),
            ("Le Teich", -1.0085, 44.6272),
            ("Lège-Cap-Ferret", -1.2068, 44.7167),
            ("Sanguinet", -1.0449, 44.4755),
            ("Lac de Cazaux", -1.1453, 44.4779),
        ],
        # Coordonnées relevées sur Google Maps, complétées au géocodeur IGN et
        # OpenStreetMap.
        "helipads": [
            ("Aérodrome de La Teste", -1.116442178115823, 44.596643236436016),
            ("Hôpital Jean-Hameau (La Teste-de-Buch)", -1.1669, 44.6327),
            ("Base aérienne 120 (Cazaux)", -1.1510, 44.5421),
            ("Port d'Arcachon", -1.1479, 44.6599),
            ("Jetée de Bélisaire (Cap Ferret)", -1.2383, 44.6565),
            ("Port d'Andernos-les-Bains", -1.1111, 44.7449),
        ],
        # Zones sans végétation (nom, lon, lat, rayon_m) : les pistes et bandes
        # enherbées des aérodromes sont vertes dans l'ortho et seraient sinon
        # boisées. Rayon à ajuster visuellement.
        "exclusions": [
            ("Aérodrome de La Teste-de-Buch (LFCH)", -1.1117, 44.5942, 550),
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
        "start_heading": 180,       # face au sud : vers le fond de la vallée
        "height_min": 0.0,
        "grid": 1024,      # montagne : relief ~24 m au lieu de ~48 (comme ossau)
        "ortho_px": 5000,  # ortho ~4,9 m/px au lieu de ~12 (limite serveur IGN : 5010)
        "title": "Cauterets - Gavarnie (chemin des cascades, Pont d'Espagne, cirque de Gavarnie)",
        "landmarks": [
            ("Cauterets", -0.1124, 42.8903),
            ("Chemin des cascades", -0.1246, 42.8601),  # cascade du Pas de l'Ours
            ("Pont d'Espagne", -0.1398, 42.8509),  # site réel (confluence, parking, ~1496 m)
            ("Lac de Gaube", -0.1465, 42.8400),
            ("Vignemale", -0.1456, 42.7700),
            ("Luz-Saint-Sauveur", 0.0000, 42.8730),
            ("Gèdre", 0.0175, 42.7847),
            ("Gavarnie", -0.0086, 42.7335),
            ("Brèche de Roland", -0.0386, 42.6975),
            ("Cirque de Gavarnie", -0.0090, 42.6963),
            # Lieux ajoutés (coordonnées recoupées IGN + OpenStreetMap).
            ("Lac d'Estom", -0.1001, 42.8063),
            ("Piméné", 0.0216, 42.7356),
            ("Pic du Marboré", -0.0171, 42.6858),
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
        ],
        # Hélistations : bases de secours en vallée (PGHM Cauterets, CRS de Gavarnie,
        # Luz, parking du Pont d'Espagne) et trois DZ de refuge d'altitude, fidèles au
        # rôle de l'Alouette II en montagne (secours et ravitaillement des refuges).
        "helipads": [
            ("Cauterets", -0.1120, 42.8880),
            ("Pont d'Espagne", -0.1398, 42.8509),
            ("Gavarnie", -0.0090, 42.7330),
            ("Luz-Saint-Sauveur", 0.0000, 42.8730),
            ("Refuge des Oulettes de Gaube", -0.1412, 42.7929),
            ("Refuge des Sarradets", -0.0333, 42.6959),
            ("Refuge de Bayssellance", -0.1241, 42.7794),
        ],
    },
    # Pic du Midi de Bigorre (Hautes-Pyrénées) : l'observatoire (~2877 m) et son
    # antenne, le col du Tourmalet et la station de La Mongie, avec le lac de Payolle
    # et Sainte-Marie-de-Campan au nord. Haute montagne sans mer ; comme Ossau et
    # Cauterets on garde les petites constructions (cabanes, granges) utiles au
    # repérage. Emprise compacte (~18x19 km), calée sur le modèle ossau (grille 1024,
    # ortho 4096). Le Pic est ~36 km à l'est du bord de la carte ossau, donc hors
    # emprise : il fallait une carte propre plutôt qu'un simple landmark.
    "bigorre": {
        "bbox": (0.05, 0.27, 42.85, 43.02),
        "recolor_sea": False,
        "start": (0.178, 42.909),  # La Mongie, replat de station (~1785 m)
        "start_heading": 270,      # face à l'ouest : le Tourmalet et le pic du Midi
        "height_min": 0.0,
        "grid": 1024,      # mailles ~17,5 m (comme ossau)
        "ortho_px": 4096,  # hauteur d'ortho ~4,4 m/px (comme ossau)
        "title": "Pic du Midi de Bigorre (observatoire, col du Tourmalet, La Mongie)",
        "landmarks": [
            ("Pic du Midi de Bigorre", 0.1411, 42.9369),
            ("Col du Tourmalet", 0.1447, 42.9075),
            ("La Mongie", 0.1783, 42.9094),
            ("Lac de Payolle", 0.2158, 42.9469),
            ("Barèges", 0.0658, 42.8983),
            ("Sainte-Marie-de-Campan", 0.1983, 42.9781),
            ("Campan", 0.1777, 43.0169),  # mairie : le point précédent était sur un versant
            # Lieux ajoutés (coordonnées recoupées IGN + OpenStreetMap).
            ("Pic de Montaigu", 0.0688, 42.9837),
            ("Pène Nère", 0.1730, 42.9556),
            ("Pic de Sencours", 0.1508, 42.9269),
            ("Le Chiroulet", 0.0890, 42.9619),
            ("Gripp", 0.2224, 42.9466),
            ("Jardin botanique du Tourmalet", 0.1054, 42.8961),
            ("Lac d'Oncet", 0.1347, 42.9292),
            ("Cascade du Garet", 0.2053, 42.9250),
            ("Aygues-Cluses", 0.1471, 42.8772),
            # Lieux ajoutés (recherche Overpass sur l'emprise, coordonnées OpenStreetMap).
            ("Lesponne", 0.1384, 43.0061),
            ("Super Barèges", 0.1315, 42.9053),
            ("Col de Madamète", 0.1404, 42.8576),
            ("Lacs de Bastan", 0.2095, 42.8521),
            ("Col de Bastan", 0.2189, 42.8575),
            ("Pic de Bastan", 0.2007, 42.8660),
            ("Barrage du Herraou", 0.0614, 42.9594),
            ("Mounaques de Campan", 0.1774, 43.0167),
            ("L'appel sauvage", 0.0771, 42.8941),
            ("La pépinière céleste", 0.0697, 42.8963),
            ("Col d'Aouet", 0.1481, 42.9476),
            ("Pont de la Gaubie", 0.1048, 42.8953),
            ("Souriche", 0.0745, 42.9016),
            ("Saint-Roch", 0.1889, 43.0059),
            ("Col de Tracens", 0.1345, 42.8647),
        ],
        "helipads": [
            ("La Mongie", 0.1783, 42.9094),
            ("Col du Tourmalet", 0.1447, 42.9075),
            ("Observatoire du Pic du Midi", 0.1411, 42.9369),
            ("Lac de Payolle", 0.2158, 42.9469),
            ("Barrage du Herraou", 0.0614, 42.9594),
            ("Super Barèges", 0.1315, 42.9053),
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
    },
    # Dax : la ville, l'Adour, l'aérodrome de Seyresse et le musée de l'ALAT
    # (Aviation légère de l'Armée de terre) au sud, Saint-Paul-lès-Dax et les
    # Thermes au nord. Vallée de l'Adour, lisière de la forêt landaise : terrain
    # plat, pas de mer.
    "dax": {
        "bbox": (-1.16, -0.98, 43.62, 43.82),
        "recolor_sea": False,
        "start": (-1.0692, 43.6884),  # aérodrome de Dax-Seyresse, devant le musée de l'ALAT
        "start_heading": 0,           # face au nord : vers Dax et l'Adour
        # À ajuster visuellement : petites annexes du bâti pavillonnaire.
        "height_min": 3.0,
        "grid": 1024,      # mailles ~14,1 m au lieu de ~28,3 m (défaut 512)
        "ortho_px": 5000,  # ~5-6 m/px (limite serveur IGN : 5010)
        "title": "Dax (Adour, aérodrome de Seyresse et musée de l'ALAT, thermes)",
        "landmarks": [
            ("Dax", -1.0602, 43.7007),
            ("Cathédrale Notre-Dame de Dax", -1.0530, 43.7088),
            ("Fontaine chaude", -1.0526, 43.7110),
            ("Pont Vieux", -1.0541, 43.7125),
            ("Les Arènes de Dax", -1.0679, 43.7013),
            ("Aérodrome de Dax-Seyresse", -1.0692, 43.6884),
            ("Musée de l'ALAT", -1.0635, 43.6922),
            ("Saint-Paul-lès-Dax", -1.0511, 43.7250),  # mairie : centre-ville réel
            ("Les Thermes de Dax", -1.0686, 43.7664),
            ("Hippodrome de Dax", -1.0669, 43.7625),
            ("Narrosse", -1.0071, 43.6950),
            ("Yzosse", -1.0152, 43.7121),
            ("Seyresse", -1.0635, 43.6840),
            ("Oeyreluy", -1.0808, 43.6749),
            ("Tercis-les-Bains", -1.1125, 43.6725),
            ("Bénesse-lès-Dax", -1.0377, 43.6428),
            # Lieux ajoutés (coordonnées géocodeur IGN et OpenStreetMap).
            ("Mées", -1.1171, 43.7075),
            ("Angoumé", -1.1380, 43.6884),
            ("Heugas", -1.0808, 43.6405),
            ("Saugnac-et-Cambran", -0.9912, 43.6738),
            ("Candresse", -0.9814, 43.7116),
            ("Gourbera", -1.0506, 43.8007),
            ("Berceau de Saint-Vincent-de-Paul", -1.0105, 43.7453),
            ("Parc Théodore Denis", -1.0511, 43.7125),
            ("Stade Maurice Boyau", -1.0458, 43.7121),
            ("Casino de Dax", -1.0589, 43.7111),
            ("Le Splendid", -1.0549, 43.7115),
            ("Le Sablar", -1.0535, 43.7163),
            ("Lac de Christus", -1.0688, 43.7289),
            ("Golf de Saint-Paul-lès-Dax", -1.0947, 43.7341),
        ],
        # Coordonnées relevées sur OpenStreetMap (docs/HELIPADS.tsv).
        "helipads": [
            ("Aérodrome de Dax-Seyresse / musée de l'ALAT", -1.0692, 43.6884),
            ("Centre hospitalier de Dax", -1.0416, 43.7112),
        ],
        # Piste et abords de l'aérodrome : herbe verte dans l'ortho, sinon boisée
        # par erreur (comme l'aérodrome de La Teste à Arcachon).
        "exclusions": [
            ("Aérodrome de Dax-Seyresse (LFBE)", -1.069, 43.688, 550),
        ],
    },
}

DEFAULT_ZONE = "ossau"
