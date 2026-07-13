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
        ],
        "helipads": [
            # Fabrèges : hameau au bord du lac, aire de départ du vol.
            ("Fabrèges", -0.3972, 42.8799),
            # Sommet du pic du Midi d'Ossau : point culminant de la heightmap 1024
            # (~2847 m). Centrer sur le maximum local évite d'enterrer le disque
            # côté amont ; la plate-forme du moteur (heightAt + jupe) fait le reste.
            ("Pic du Midi d'Ossau", -0.4382796, 42.8430108),
        ],
    },
    # Côte basco-landaise, de Bayonne / Anglet (embouchure de l'Adour) au sud
    # jusqu'à Vieux-Boucau-les-Bains au nord. Bord de mer : l'océan à l'ouest est
    # hors couverture BD ORTHO (blanc) et se fait aplanir en mer unie.
    "cote-landes": {
        "bbox": (-1.62, -1.30, 43.46, 43.81),
        "recolor_sea": True,
        "start": (-1.43, 43.66),  # arrière-plage plate vers Hossegor / Capbreton
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
            # Lieux ajoutés (coordonnées IGN, iles et lacs recoupés OpenStreetMap).
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
            # Lieux ajoutés (coordonnées IGN, ile et lac recoupés OpenStreetMap).
            ("Île aux Oiseaux", -1.1780, 44.6985),
            ("Banc d'Arguin", -1.2430, 44.5855),
            ("Phare du Cap Ferret", -1.2488, 44.6460),
            ("Le Teich", -1.0085, 44.6272),
            ("Lège-Cap-Ferret", -1.2068, 44.7167),
            ("Sanguinet", -1.0449, 44.4755),
            ("Lac de Cazaux", -1.1453, 44.4779),
        ],
        # Coordonnées relevées sur Google Maps.
        "helipads": [
            ("Aérodrome de La Teste", -1.116442178115823, 44.596643236436016),
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
            ("Pont d'Espagne", -0.1437, 42.8556),
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
            ("Campan", 0.1747, 43.0000),
            # Lieux ajoutés (coordonnées recoupées IGN + OpenStreetMap).
            ("Pic de Montaigu", 0.0688, 42.9837),
            ("Pène Nère", 0.1730, 42.9556),
            ("Pic de Sencours", 0.1508, 42.9269),
            ("Le Chiroulet", 0.0890, 42.9619),
        ],
        "helipads": [
            ("La Mongie", 0.1783, 42.9094),
            ("Col du Tourmalet", 0.1447, 42.9075),
            ("Observatoire du Pic du Midi", 0.1411, 42.9369),
            ("Lac de Payolle", 0.2158, 42.9469),
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
}

DEFAULT_ZONE = "ossau"
