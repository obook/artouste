"""
capbreton
Capbreton, Hossegor et le lac marin, en emprise serrée : le port et l'estacade
du Boucarot, la plage centrale, la passe vers le lac d'Hossegor et les deux
hélipads. Bord de mer : l'océan à l'ouest est hors couverture BD ORTHO (blanc)
et se fait aplanir en mer unie.

Cette carte est l'insert fin de cote-landes, qui couvre la même côte de Labenne
à Vieux-Boucau mais sur 28 km de haut, donc à 3,5 m/px seulement. Or la finesse
au sol perçue dépend du grossissement de la texture : à 30 m sol, l'écran
couvre une soixantaine de mètres, soit 4,7 cm par pixel écran ; 3,5 m/px sont
alors grossis 74 fois et le sol devient une bouillie. Ici, 0,42 m/px ne sont
grossis que 9 fois.

Le partage est donc celui de dax et dax-arene : cote-landes pour longer la côte
en croisière, capbreton pour l'approche, le vol bas et le poser.
"""

ZONE = {
    "bbox": (-1.460, -1.415, 43.640, 43.682),
    "recolor_sea": True,
    # Hélipad de Capbreton, comme sur cote-landes : viser directement les
    # coordonnées du pad évite que le calage "hélipad le plus proche" du moteur
    # (ApplicationScene.cpp) bascule d'un pad à l'autre entre deux régénérations.
    "start": (-1.4457112839188175, 43.65393627677582),
    "start_heading": 0,       # face au nord : la passe, le lac et Hossegor
    "grid": 512,       # maille ~9 m, contre ~25-38 m sur cote-landes
    "ortho_px": 11000,  # ortho ~0,42 m/px ; mosaique WMS 2x3
    "title": "Capbreton, Hossegor et le lac marin",
    # Repères repris de cote-landes, où ils étaient déjà recoupés IGN et
    # OpenStreetMap. L'estacade vient de la clé "exclusions" de cette zone.
    "landmarks": [
        ("Capbreton", -1.4310, 43.6420),
        ("Hossegor", -1.4276, 43.6589),  # mairie
        ("Lac d'Hossegor", -1.4287, 43.6722),
        ("Estacade de Capbreton", -1.4488, 43.6552),
    ],
    "helipads": [
        ("Capbreton", -1.4457112839188175, 43.65393627677582),
        ("Hossegor", -1.4438385382046726, 43.661316497891036),
    ],
    # Estacade de Capbreton (jetée sud du chenal du Boucarot) : bande de sable
    # et d'eau mal classée par le scatter de végétation, un arbre y poussait.
    "exclusions": [
        ("Estacade de Capbreton", -1.4488, 43.6552, 200),
    ],
    # Balise HAPI sur le pad de départ. Azimut = cap de départ (aucune piste
    # réelle ici) ; pente 6 %, valeur usuelle pour une hélistation.
    "hapi": [
        ("Capbreton", -1.4457112839188175, 43.65393627677582, 0, 6),
    ],
}
