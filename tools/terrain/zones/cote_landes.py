"""
cote-landes
Côte landaise, de Labenne au sud jusqu'à Vieux-Boucau-les-Bains au nord, avec
Capbreton, Hossegor et son lac au centre. Bord de mer : l'océan à l'ouest est
hors couverture BD ORTHO (blanc) et se fait aplanir en mer unie.

L'emprise couvrait au départ toute la côte basco-landaise, jusqu'à Bayonne et
Biarritz au sud-ouest. Deux défauts se cumulaient. D'abord la côte est
diagonale, du sud-ouest au nord-est : le rectangle qui contenait à la fois le
Rocher de la Vierge et Vieux-Boucau était énorme et rempli d'océan, 73 % de
l'orthophoto. Ensuite ortho_px fixe la HAUTEUR en pixels, donc la finesse au
sol ne dépendait que de l'étendue nord-sud, alors de 38,7 km. Résultat :
7,7 m/px, la carte la plus grossière du jeu après arcachon.

En abandonnant le bloc basque, au sud de Labenne, l'étendue nord-sud tombe à
27,8 km sans perdre un seul hélipad. Biarritz et le Rocher de la Vierge
mériteraient leur propre carte serrée plutôt que de dégrader celle-ci.
"""

ZONE = {
    "bbox": (-1.50, -1.30, 43.55, 43.80),
    "recolor_sea": True,
    # Calé sur l'hélipad de Capbreton (voir "helipads" ci-dessous) : l'ancien
    # indice "arrière-plage plate vers Hossegor / Capbreton" tombait au ras de
    # l'eau (0 m d'altitude, bord du lac d'Hossegor), et le calage "hélipad le
    # plus proche" (ApplicationScene.cpp) devenait instable d'une régénération
    # à l'autre. On vise donc directement les coordonnées exactes du pad.
    "start": (-1.4457112839188175, 43.65393627677582),  # Capbreton
    "start_heading": 0,       # face au nord : la côte file vers Vieux-Boucau
    "grid": 1024,      # maille ~16x27 m sur la nouvelle emprise
    "ortho_px": 8000,  # ortho ~3,5 m/px au lieu de ~7,8 ; mosaique WMS 1x2
    # La carte livrée dans assets/terrain/cote-landes/ a été obtenue en
    # recadrant l'ancienne (crop_zombie_map.py) plutôt qu'en régénérant, pour
    # éviter de repasser par l'API altimétrie, point par point et très lente.
    # Son relief garde donc la maille de l'ancienne emprise, 640x732 mailles
    # d'environ 25x38 m. Une régénération complète par fetch_terrain.py
    # appliquerait le grid ci-dessus et affinerait aussi le relief : c'est un
    # progrès, pas une régression, mais terrain.txt en sortirait différent.
    "title": "côte landaise (Labenne -> Vieux-Boucau)",
    "landmarks": [
        ("Ondres", -1.4479, 43.5612),  # mairie/église
        ("Labenne", -1.4260, 43.5947),  # mairie/église
        ("Capbreton", -1.4310, 43.6420),
        ("Hossegor", -1.4276, 43.6589),  # mairie
        ("Seignosse", -1.3739, 43.6897),  # mairie
        ("Vieux-Boucau", -1.4010, 43.7880),
        # Lieux ajoutés (coordonnées IGN, îles et lacs recoupés OpenStreetMap).
        ("Soustons", -1.3284, 43.7539),  # mairie
        ("Lac d'Hossegor", -1.4287, 43.6722),
        ("Étang de Soustons", -1.3292, 43.7703),
        ("Étang Blanc", -1.3653, 43.7094),
        ("St-Vincent-de-Tyrosse", -1.3055, 43.6667),
        ("Bénesse-Maremne", -1.3594, 43.6342),  # mairie
        ("Marais d'Orx", -1.3972, 43.6007),
        # Milieu de la jetée, mesuré sur l'axe BD TOPO. Le point venait de la
        # carte capbreton, retirée le 27/07/2026, et valait (-1.4488, 43.6552) :
        # 110 m au-delà du bout de la jetée, en pleine mer. L'étiquette flottait
        # sur l'eau et on cherchait la jetée à côté.
        ("Estacade", -1.446238, 43.655183),
        # Ajoutés jadis à la main dans landmarks.txt sans passer par ici : une
        # régénération les effaçait. Le fichier est produit, la zone est la
        # source, rien ne doit vivre que dans le fichier.
        ("Saubrigues", -1.315111, 43.610662),
        ("Seignosse Océan", -1.4339, 43.6981),
        ("Étang de Pinsolle", -1.4071, 43.7719),
        ("Les Oyats", -1.4215, 43.7246),
    ],
    # Coordonnées relevées sur Google Maps.
    "helipads": [
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
