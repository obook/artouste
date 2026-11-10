"""
test_fetch_buildings.py
Vérifie que les trous d'une emprise BD TOPO (cour, arène) sont reliés au
contour en un seul anneau, sans le boucher. Rien n'est téléchargé.

  python3 tools/test_fetch_buildings.py

Auteur : O. Booklage
Licence : GPL v2
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fetch_buildings import relier_trous, surface_signee


def _carre(x0, y0, cote):
    return [(x0, y0), (x0 + cote, y0), (x0 + cote, y0 + cote), (x0, y0 + cote)]


def test_un_trou():
    # 100 - 4 : la surface du trou est retirée, quel que soit son sens.
    for trou in (_carre(4, 4, 2), _carre(4, 4, 2)[::-1]):
        anneau = relier_trous(_carre(0, 0, 10), [trou])
        assert len(anneau) == 4 + 4 + 2
        assert abs(surface_signee(anneau) - 96) < 1e-9


def test_deux_trous():
    anneau = relier_trous(_carre(0, 0, 10)[::-1], [_carre(1, 1, 2), _carre(6, 6, 2)])
    assert abs(abs(surface_signee(anneau)) - 92) < 1e-9


if __name__ == "__main__":
    test_un_trou()
    test_deux_trous()
    print("ok")
