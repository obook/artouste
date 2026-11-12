"""
test_fetch_lampadaires.py
Vérifie le tri des lampadaires par proximité du bâti : on garde ceux qui ont
un sommet de bâtiment à moins du rayon, y compris quand le sommet est dans la
case voisine. Rien n'est téléchargé.

  python3 tools/test_fetch_lampadaires.py

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fetch_lampadaires import M_PAR_DEG_LAT, altitude_sur_tablier, pres_du_bati

M_PAR_DEG_LON = M_PAR_DEG_LAT  # équateur : un degré vaut autant en lon qu'en lat


def _deg(metres):
    return metres / M_PAR_DEG_LAT


def test_pres_du_bati():
    maison = [(_deg(100), _deg(100))]
    points = [(_deg(100), _deg(130)),   # 30 m : gardé
              (_deg(149), _deg(100)),   # 49 m, case voisine : gardé
              (_deg(100), _deg(160)),   # 60 m : écarté
              (_deg(900), _deg(900))]   # loin : écarté
    assert pres_du_bati(points, maison, 50.0, M_PAR_DEG_LON) == points[:2]
    assert pres_du_bati(points, [], 50.0, M_PAR_DEG_LON) == []


def test_altitude_sur_tablier():
    # Tablier de 100 m vers l'est, 6 m de demi-largeur, de 10 m à 20 m d'altitude.
    axe = [(6.0, [(0.0, 0.0, 10.0), (_deg(100), 0.0, 20.0)])]
    assert altitude_sur_tablier((_deg(50), _deg(5)), axe, M_PAR_DEG_LON) == 15.0
    assert math.isnan(altitude_sur_tablier((_deg(50), _deg(7)), axe, M_PAR_DEG_LON))
    assert math.isnan(altitude_sur_tablier((_deg(120), 0.0), axe, M_PAR_DEG_LON))


if __name__ == "__main__":
    test_pres_du_bati()
    test_altitude_sur_tablier()
    print("ok")
