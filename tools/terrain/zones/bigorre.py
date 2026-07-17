# -*- coding: utf-8 -*-
"""
bigorre
Pic du Midi de Bigorre (Hautes-Pyrénées) : l'observatoire (~2877 m) et son
antenne, le col du Tourmalet et la station de La Mongie, avec le lac de Payolle
et Sainte-Marie-de-Campan au nord. Haute montagne sans mer ; comme Ossau et
Cauterets on garde les petites constructions (cabanes, granges) utiles au
repérage. Emprise compacte (~18x19 km), calée sur le modèle ossau (grille 1024,
ortho 5000). Le Pic est ~36 km à l'est du bord de la carte ossau, donc hors
emprise : il fallait une carte propre plutôt qu'un simple landmark.
"""

ZONE = {
    "bbox": (0.05, 0.27, 42.85, 43.02),
    "recolor_sea": False,
    "start": (0.178, 42.909),  # La Mongie, replat de station (~1785 m)
    "start_heading": 270,      # face à l'ouest : le Tourmalet et le pic du Midi
    "height_min": 0.0,
    "grid": 1024,      # mailles ~17,5 m (comme ossau)
    "ortho_px": 5000,  # ortho ~3,8 m/px (limite serveur IGN : 5010)
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
    # Balise HAPI (indicateur de pente d'approche pour hélicoptère, voir
    # media/gt_installation_hapi.pdf) sur le pad de départ. Azimut = cap de
    # départ (aucune piste réelle recensée ici, contrairement à Dax) ; pente
    # 6 % : valeur usuelle pour une hélistation.
    "hapi": [
        ("La Mongie", 0.1783, 42.9094, 270, 6),
    ],
}
