#!/usr/bin/env python3
"""
Nom du fichier : fetch_textures_pau.py
Description : Récupère les trois morceaux d'herbe qui texturent le décor de la
              base de Pau, depuis le service WMS de l'IGN.
Auteur : O. Booklage
Date : Août 2026
Licence : GPL v2

Le décor (decor_base_pau.py) pave son herbe avec de VRAIS morceaux
d'orthophoto à 25 cm : agrandir la photo d'ensemble de la carte, qui est à
1,80 m par pixel, ne donne que des taches floues aux coutures visibles.

Les trois emplacements ci-dessous ont été choisis une fois pour toutes sur des
zones d'herbe uniformes de la plateforme, à l'intérieur du masque de la zone
militaire. Ils sont figés ici pour que deux exécutions rendent le même décor :
prélever ailleurs changerait la teinte du gazon.

Données IGN sous Licence Ouverte Etalab 2.0.

Usage : python3 -m decor.fetch_textures_pau [dossier-de-sortie]
        (défaut : $ARTOUSTE_DECOR_TEXTURES, sinon /tmp)
"""

import math
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))
from terrain.ortho import request_map

# Centre de chaque prélèvement, en degrés.
#
# HORS de la zone militaire : l'IGN y floute son imagerie à la source, et un
# prélèvement fait dedans revient à 8 m par pixel agrandi, pas à 25 cm. Ces
# trois-là sont pris dans la campagne alentour, à 700 ou 900 m de la
# plateforme, à plus de 150 m du bord de la zone floutée. Choisis sur
# Choisis sur l'orthophoto de la carte en exigeant 99,5 % de pixels d'herbe
# dans la fenêtre, puis départagés sur la donnée fine elle-même : un coin de
# piste, un carré de terre nue ou un champ de culture à rangs marqués se
# verraient au pavage. Ce sont des prairies, l'entour réel de la plateforme :
# aucune bande d'herbe de l'aérodrome ne fait 128 m de côté.
EMPLACEMENTS = (
    ("herbe",  -0.4238202, 43.3657083),
    ("herbe2", -0.4192849, 43.3637679),
    ("herbe3", -0.4088802, 43.3617305),
)

# 512 pixels pour 128 m au sol, soit les 25 cm par pixel qu'attend le décor.
COTE_PX = 512
COTE_M = 128.0


def prelever(nom, lon, lat, sortie):
    """Un carré de COTE_M mètres centré sur (lon, lat), rendu à COTE_PX pixels."""
    demi_lat = 0.5 * COTE_M / 111320.0
    demi_lon = 0.5 * COTE_M / (111320.0 * math.cos(math.radians(lat)))
    image = request_map(lat - demi_lat, lon - demi_lon,
                        lat + demi_lat, lon + demi_lon,
                        COTE_PX, COTE_PX)
    chemin = sortie / ("tex_%s.png" % nom)
    image.save(chemin)
    print("%s : %d x %d px, %.2f m/px" % (chemin, image.width, image.height,
                                          COTE_M / COTE_PX))


def main():
    defaut = os.environ.get("ARTOUSTE_DECOR_TEXTURES", "/tmp")
    sortie = Path(sys.argv[1] if len(sys.argv) > 1 else defaut)
    sortie.mkdir(parents=True, exist_ok=True)
    for nom, lon, lat in EMPLACEMENTS:
        prelever(nom, lon, lat, sortie)


if __name__ == "__main__":
    main()
