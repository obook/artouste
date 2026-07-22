# -*- coding: utf-8 -*-
"""
dax
Dax : la ville, l'Adour, l'aérodrome de Seyresse et le musée de l'ALAT
(Aviation légère de l'Armée de terre) au sud, Saint-Paul-lès-Dax et les
Thermes au nord. Vallée de l'Adour, lisière de la forêt landaise : terrain
plat, pas de mer.

ATTENTION : cette entrée décrit l'emprise BRUTE utilisée pour amorcer les
données (relief + première ortho). La carte livrée dans assets/terrain/dax/
est ensuite recadrée sur le centre-ville, l'aérodrome, Saint-Paul-lès-Dax et
les Thermes (voir tools/terrain/crop_zombie_map.py, --center-x 842
--center-z 1400 --half-x 1800 --half-z 2500) pour un sol net avec un budget
de pixels WMS raisonnable. Relancer `fetch_terrain.py dax` régénère la
grande emprise d'origine et écrase ce recadrage : il faudrait ensuite
rejouer la commande de recadrage ci-dessus.
"""

ZONE = {
    "bbox": (-1.16, -0.98, 43.62, 43.82),
    "recolor_sea": False,
    "start": (-1.0692, 43.6884),  # aérodrome de Dax-Seyresse, devant le musée de l'ALAT
    "start_heading": 0,           # face au nord : vers Dax et l'Adour
    # À ajuster visuellement : petites annexes du bâti pavillonnaire.
    "height_min": 3.0,
    "grid": 1024,      # mailles ~14,1 m au lieu de ~28,3 m (défaut 512)
    "ortho_px": 10000,  # ~2,2 m/px ; mosaique WMS 2x2 (limite serveur IGN : 5010/tuile)
    "title": "Dax (Adour, aérodrome de Seyresse et musée de l'ALAT, thermes)",
    "landmarks": [
        ("Dax", -1.0602, 43.7007),
        ("Cathédrale Notre-Dame de Dax", -1.0530, 43.7088),
        ("Fontaine chaude", -1.0526, 43.7110),
        ("Pont Vieux", -1.0541, 43.7125),
        ("Les Arènes de Dax", -1.0498, 43.7131),
        ("Aérodrome de Dax-Seyresse", -1.0692, 43.6884),
        ("Musée de l'ALAT", -1.0635, 43.6922),
        ("Saint-Paul-lès-Dax", -1.0511, 43.7250),  # mairie : centre-ville réel
        ("Les Thermes de Dax", -1.0649, 43.7290),  # Thermes de Christus, le plus grand établissement
        ("Hippodrome de Dax", -1.0669, 43.7625),
        ("Narrosse", -1.0071, 43.6950),
        ("Yzosse", -1.0152, 43.7121),
        ("Seyresse", -1.0635, 43.6840),
        ("Oeyreluy", -1.0795, 43.6723),  # mairie
        ("Tercis-les-Bains", -1.1090, 43.6702),  # bourg de Palisse (mairie/église)
        ("Bénesse-lès-Dax", -1.0377, 43.6428),
        # Lieux ajoutés (coordonnées géocodeur IGN et OpenStreetMap).
        ("Mées", -1.1106, 43.7025),  # mairie/église
        ("Angoumé", -1.1352, 43.6904),  # mairie
        ("Heugas", -1.0808, 43.6405),
        ("Saugnac-et-Cambran", -0.9935, 43.6710),  # bourg de Saugnac (mairie/église)
        ("Candresse", -0.9814, 43.7116),
        ("Gourbera", -1.0488, 43.8041),  # mairie/église
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
    # Balise HAPI (indicateur de pente d'approche pour hélicoptère, voir
    # media/gt_installation_hapi.pdf) sur l'hélipad de l'aérodrome. Azimut
    # 70° : aligné sur la piste bitumée 07/25 de Dax-Seyresse (aérodrome
    # école de l'ALAT), approche vers l'est (QFU 07). Pente 6 % : valeur
    # usuelle pour une hélistation, faute de relevé d'obstacles réel.
    # Position et calage provisoires, à affiner sur place (méthode IGN/OSM).
    "hapi": [
        ("Aérodrome de Dax-Seyresse", -1.0692, 43.6884, 70, 6),
    ],
}
