# -*- coding: utf-8 -*-
"""
paris
Paris intra-muros (dans les limites du boulevard périphérique) : la Seine, l'Île
de la Cité et l'Île Saint-Louis au centre, de la tour Eiffel et du Bois de
Boulogne à l'ouest jusqu'au Bois de Vincennes et au château de Vincennes à
l'est, de Montmartre au nord jusqu'à Montparnasse et Denfert-Rochereau au sud.
Terre intérieure : pas d'océan, la Seine garde sa vraie imagerie (pas de
recoloration de mer).

Emprise volontairement resserrée à la ville elle-même (sans les communes de la
petite couronne) : Paris est bien plus dense en bâtiments que Bordeaux à
surface égale, donc on limite l'aire de la zone (~175 km² d'emprise contre
~675 km² pour "Bordeaux et son agglomération") et on relève le seuil de hauteur
(voir "height_min" ci-dessous) pour garder un buildings.bin comparable.
"""

ZONE = {
    "bbox": (2.224, 2.470, 48.815, 48.902),
    "recolor_sea": False,
    # Le départ se cale toujours sur l'hélipad réel le plus proche de ce repère
    # (voir ApplicationScene::rebuildTerrain) : on vise donc directement l'Héliport
    # de Paris - Issy-les-Moulineaux, le seul héliport de l'agglomération. Le point
    # est celui de l'aire de poser elle-même : l'ancien tombait 424 m au sud-ouest,
    # sur la piste d'athlétisme du parc des sports d'Issy.
    "start": (2.274772, 48.833076),
    # Ville encore plus dense que Bordeaux à surface égale (183 571 bâtiments au
    # seuil de 5 m, contre 159 000 pour Bordeaux sur une emprise 4x plus grande).
    # Le bâti parisien est presque partout un immeuble haussmannien (largement
    # > 10 m) : un seuil relevé à 10 m n'écarte donc quasiment aucun vrai
    # immeuble, seulement kiosques, pavillons de jardin et petites annexes, et
    # ramène le fichier à 124 235 bâtiments, 13,6 Mo -- comparable à Bordeaux.
    "height_min": 10.0,
    "grid": 1024,      # mailles ~17 m au lieu de ~34 m (défaut 512)
    # Zone plus large que haute (18,0 km E-O contre 9,7 km N-S), à l'inverse des
    # autres cartes : ortho_px fixe la HAUTEUR en pixels, donc c'est la LARGEUR
    # (ortho_px x aspect) qui vient buter sur la limite serveur IGN (5010 px).
    # 2688 -> largeur ~5000 px, hauteur ~2688 px, soit ~3,6 m/px (même méthode
    # -- pousser jusqu'à la limite serveur -- que Bordeaux, Dax et Arcachon).
    "ortho_px": 2688,
    "title": "Paris (tour Eiffel, Notre-Dame, Montmartre, Champs-Élysées)",
    "landmarks": [
        ("Paris", 2.3488, 48.8534),  # point zéro des routes de France, devant Notre-Dame
        ("Tour Eiffel", 2.2945, 48.8584),
        ("Notre-Dame de Paris", 2.3499, 48.8530),
        ("Arc de Triomphe", 2.2950, 48.8738),
        ("Musée du Louvre", 2.3376, 48.8606),
        ("Sacré-Coeur", 2.3431, 48.8867),
        ("Panthéon", 2.3459, 48.8462),
        ("Place de la Concorde", 2.3212, 48.8656),
        ("Hôtel des Invalides", 2.3125, 48.8566),
        ("Opéra Garnier", 2.3316, 48.8719),
        ("Place de la Bastille", 2.3696, 48.8532),
        ("Centre Pompidou", 2.3522, 48.8606),
        ("Tour Montparnasse", 2.3220, 48.8421),
        ("Jardin du Luxembourg", 2.3372, 48.8462),
        ("Champ de Mars", 2.2988, 48.8556),
        ("Gare du Nord", 2.3553, 48.8809),
        ("Gare de Lyon", 2.3739, 48.8447),
        ("Gare Montparnasse", 2.3203, 48.8412),
        ("Cimetière du Père-Lachaise", 2.3934, 48.8614),
        ("Parc des Buttes-Chaumont", 2.3822, 48.8809),
        ("Bois de Boulogne", 2.2500, 48.8642),
        ("Hippodrome de Longchamp", 2.2384, 48.8636),
        ("Bois de Vincennes", 2.4322, 48.8290),
        ("Château de Vincennes", 2.4378, 48.8422),
        ("Île de la Cité", 2.3470, 48.8555),
        ("Île Saint-Louis", 2.3565, 48.8514),
    ],
    # Héliport de Paris - Issy-les-Moulineaux (le seul héliport de l'agglomération
    # parisienne) et les hôpitaux intra-muros dotés d'une hélistation connue.
    # Coordonnées relevées au géocodeur IGN et sur OpenStreetMap ; à affiner si
    # besoin. Celle de l'héliport, elle, est relevée sur l'aire de poser et fait
    # foi : c'est le pad de départ de la carte.
    "helipads": [
        ("Héliport de Paris (Issy-les-Moulineaux)", 2.274772, 48.833076),
        ("Hôpital européen Georges-Pompidou", 2.2748, 48.8397),
        ("Hôpital Lariboisière", 2.3531, 48.8829),
        ("Hôpital de la Pitié-Salpêtrière", 2.3653, 48.8371),
    ],
    # Balise HAPI (indicateur de pente d'approche pour hélicoptère, voir
    # media/gt_installation_hapi.pdf) sur le pad de départ, l'héliport de Paris.
    # Azimut = cap de départ (90°, approche vers l'est) : c'est le seul axe
    # dégagé, le terrain ouvert de l'héliport s'étendant à l'ouest de l'aire,
    # alors que les tours du Front de Seine et le périphérique bordent l'est et
    # le nord. Ce n'est pas un relevé des trouées publiées, qui suivent la Seine.
    # Pente 6 % : valeur usuelle pour une hélistation.
    "hapi": [
        ("Héliport de Paris (Issy-les-Moulineaux)", 2.274772, 48.833076, 90, 6),
    ],
}
