# -*- coding: utf-8 -*-
"""
arcachon
Bassin d'Arcachon et son arrière-pays : de la côte atlantique et du Cap Ferret
à l'ouest jusqu'à Marcheprime à l'est (vers Bordeaux), et de Lège / Arès au nord
jusqu'à Biscarrosse et ses étangs au sud. Bord de mer : l'océan, le bassin et
les grands étangs (Cazaux-Sanguinet) sont aplanis en eau unie.
"""

ZONE = {
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
        ("Biganos", -0.9768, 44.6422),  # mairie
        ("Audenge", -1.0186, 44.6839),
        ("Andernos-les-Bains", -1.1044, 44.7436),
        ("Arès", -1.1392, 44.7686),
        ("Marcheprime", -0.8538, 44.6920),  # mairie
        ("Biscarrosse", -1.1664, 44.3936),
        ("Biscarrosse-Plage", -1.2505, 44.4450),
        # Lieux ajoutés (coordonnées IGN, île et lac recoupés OpenStreetMap).
        ("Île aux Oiseaux", -1.1780, 44.6985),
        ("Banc d'Arguin", -1.2430, 44.5855),
        ("Phare du Cap Ferret", -1.2488, 44.6460),
        ("Le Teich", -1.0211, 44.6347),  # mairie
        ("Lège-Cap-Ferret", -1.1465, 44.7926),  # mairie (Lège-Bourg, siège de la commune)
        ("Sanguinet", -1.0763, 44.4837),  # mairie
        ("Lac de Cazaux", -1.1453, 44.4779),
    ],
    # Coordonnées relevées sur Google Maps, complétées au géocodeur IGN et
    # OpenStreetMap.
    "helipads": [
        ("Aérodrome de La Teste", -1.116442178115823, 44.596643236436016),
        ("Hôpital Jean-Hameau (La Teste-de-Buch)", -1.1138, 44.6132),
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
    # Balise HAPI (indicateur de pente d'approche pour hélicoptère, voir
    # media/gt_installation_hapi.pdf) sur le pad de départ. Azimut = cap de
    # départ (aucune piste réelle recensée ici, contrairement à Dax) ; pente
    # 6 % : valeur usuelle pour une hélistation.
    "hapi": [
        ("Aérodrome de La Teste", -1.116442178115823, 44.596643236436016, 330, 6),
    ],
}
