#!/usr/bin/env python3
"""
Nom du fichier : reglages.py
Description : Services WFS, résolution du masque et niveaux de gris.
Auteur : O. Booklage
Date : Août 2026
Licence : GPL v2
"""

import re


# --- Services WFS ------------------------------------------------------------
WFS_URL = "https://data.geopf.fr/wfs/ows"
WFS_LAYER_DEPT = "ADMINEXPRESS-COG-CARTO.LATEST:departement"
WFS_LAYER_VEGETATION = "BDTOPO_V3:zone_de_vegetation"
WFS_LAYER_ESSENCE = "LANDCOVER.FORESTINVENTORY.V2:formation_vegetale"
# Couches de la passe de gomme (voir gommer) : chaussées, terrains de sport et
# pistes d'aérodrome, repeints en non boisé une fois le masque assemblé.
WFS_LAYER_ROUTE = "BDTOPO_V3:troncon_de_route"
WFS_LAYER_SPORT = "BDTOPO_V3:terrain_de_sport"
WFS_LAYER_PISTE = "BDTOPO_V3:piste_d_aerodrome"

# Nombre d'entités par requête (le service pagine ; on avance de l'effectif
# réellement renvoyé, comme fetch_buildings.py).
PAGE = 1000

# Résolution du masque, en mètres par pixel. Assez fin pour que le couloir d'une
# autoroute (25 m) reste dégagé, sans faire enfler le fichier.
PX_M = 10.0

# Niveaux de gris du masque (voir le format ci-dessus).
NON_BOISE = 40
FEUILLU = 85
PIN = 130
CONIFERE = 170
MIXTE = 255

# Natures de la BD TOPO qui portent des arbres. Écartées : lande ligneuse, vigne,
# verger, marais, canne à sucre... végétation sans houppier à l'échelle du semis.
NATURES_ARBOREES = ("bois", "forêt", "peupleraie", "haie", "zone arborée")

# Natures de route goudronnées, gommées du masque. Le chemin, le sentier, la
# route empierrée et l'escalier sont laissés : ils passent SOUS le couvert en
# vraie forêt, et les gommer perforerait le semis de tranchées imaginaires.
NATURES_ROUTE = ("Type autoroutier", "Route à 2 chaussées", "Route à 1 chaussée",
                 "Bretelle", "Rond-point")

# Chaussée de repli quand la BD TOPO ne la renseigne pas (fréquent sur les
# bretelles et les ronds-points), et marge dégagée de part et d'autre :
# accotements, bandes d'arrêt, glissières. En mètres.
CHAUSSEE_MIN_M = 4.0
MARGE_ROUTE_M = 6.0

# Mot entier : "sapin" ne doit pas être pris pour un pin.
RE_PIN = re.compile(r"\bpins?\b")
# Autres essences résineuses de la BD Forêt, reconnues par mot-clé.
MOTS_CONIFERE = ("conifère", "sapin", "épicéa", "douglas", "mélèze", "cyprès",
                 "cèdre")
