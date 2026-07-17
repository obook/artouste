# -*- coding: utf-8 -*-
"""
ossau
Vallée d'Ossau (lac d'Artouste, pic du Midi d'Ossau, ~2884 m) : le lieu-titre
du projet (la turbine Artouste de l'Alouette II est nommée d'après ce massif).
Montagne sans mer.
"""

ZONE = {
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
    "ortho_px": 4970,  # ortho ~3,6 m/px (bbox plus large que haute : largeur WMS
                       # deja a ~5008 px, tout pres de la limite serveur 5010)
    "title": "vallée d'Ossau (lac d'Artouste, pic du Midi d'Ossau)",
    "landmarks": [
        ("Lac d'Artouste", -0.3325, 42.8589),
        ("Pic du Midi d'Ossau", -0.4380, 42.8430),
        ("Pic Palas", -0.3133, 42.8495),
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
    # Balise HAPI (indicateur de pente d'approche pour hélicoptère, voir
    # media/gt_installation_hapi.pdf) sur le pad de départ (Fabrèges). Azimut
    # = cap de départ (aucune piste réelle recensée ici, contrairement à
    # Dax) ; pente 6 % : valeur usuelle pour une hélistation.
    "hapi": [
        ("Fabrèges", -0.3972, 42.8799, 180, 6),
    ],
}
