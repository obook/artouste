"""
zones
Description des terrains réels disponibles. Chaque zone décrit une emprise
géographique (WGS84), son comportement vis-à-vis de la mer (recolor_sea), son
point de départ du vol et ses lieux remarquables. Pour ajouter une zone : créer
un module zones/<nom>.py sur le modèle des zones existantes, l'importer et
l'ajouter au dictionnaire ZONES ci-dessous. La sortie va dans
assets/terrain/<nom>/.

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

from .arcachon import ZONE as _ARCACHON
from .bigorre import ZONE as _BIGORRE
from .bordeaux import ZONE as _BORDEAUX
from .cauterets import ZONE as _CAUTERETS
from .cote_landes import ZONE as _COTE_LANDES
from .dax import ZONE as _DAX
from .ossau import ZONE as _OSSAU
from .paris import ZONE as _PARIS

ZONES = {
    "ossau": _OSSAU,
    "cote-landes": _COTE_LANDES,
    "arcachon": _ARCACHON,
    "cauterets": _CAUTERETS,
    "bigorre": _BIGORRE,
    "bordeaux": _BORDEAUX,
    "dax": _DAX,
    "paris": _PARIS,
}

DEFAULT_ZONE = "ossau"
