"""
test_relief_lidar.py
Vérifie le comblement des trous du LiDAR par le relief en place, et son raccord
en fondu. Rien n'est téléchargé.

Les points à tenir : un trou garde exactement le relief en place ; le laser s'y
raccorde en fondu, sans marche (des falaises de 300 m ont été livrées le 16/08
faute de ce fondu) ; loin des trous, le laser ressort intact.

  python3 tools/test_relief_lidar.py

Auteur : O. Booklage
Licence : GPL v2
"""

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from relief_lidar import RACCORD_M, combler, marches_par_zone
from terrain import config

MAILLE = 10.0  # m ; raccord de 50 mailles


def _carte(n=256):
    """Un relief de synthèse : un versant, avec une bosse."""
    z, x = np.mgrid[0:n, 0:n]
    return 1000.0 + 2.0 * x + 300.0 * np.exp(-((x - n / 2) ** 2 + (z - n / 2) ** 2) / 2000.0)


def test_sans_trou():
    """Sans trou ni aberration, les altitudes ressortent inchangées."""
    carte = _carte()
    lidar = carte + 5.0
    sortie, absent, aberrant, _ = combler(lidar, carte, MAILLE)
    assert absent == 0.0 and aberrant == 0.0
    assert np.array_equal(sortie, lidar)


def test_trou_garde_la_carte_et_raccorde():
    """Dans le trou : la carte exactement. Loin du trou : le laser exactement.
       Entre les deux : un fondu, et surtout AUCUNE marche, même quand les deux
       relevés s'écartent de 250 m (le défaut livré le 16/08)."""
    carte = _carte()
    lidar = carte + 250.0  # désaccord volontairement énorme
    lidar[:, :60] = config.NODATA - 1.0  # bande ouest sans laser

    sortie, absent, _, _ = combler(lidar, carte, MAILLE)
    assert 20.0 < absent < 30.0

    assert np.array_equal(sortie[:, :60], carte[:, :60])          # trou : la carte
    assert np.abs(sortie[:, 130:] - lidar[:, 130:]).max() < 1e-9  # loin : le laser

    # Pas de marche : au raccord, le pire dénivelé voisin reste celui du
    # terrain plus la pente du fondu (250 m sur 50 mailles = 5 m par maille).
    _, _, _, zone = combler(lidar, carte, MAILLE)
    dans, hors = marches_par_zone(sortie, zone)
    assert dans < 40.0 and hors < 40.0, (dans, hors)


def test_puits_aberrant_ecarte():
    """Un point du laser qui plonge de deux kilomètres sous le relief en place
       est une mesure fausse : lui et ses abords immédiats reviennent vers la
       carte. Une falaise de 200 m en dessous, elle, reste du laser (hors zone
       de raccord d'un trou)."""
    carte = _carte()
    lidar = carte.copy()
    lidar[50, 50] = carte[50, 50] - 2000.0
    lidar[200, 200] = carte[200, 200] - 200.0

    sortie, absent, aberrant, _ = combler(lidar, carte, MAILLE)
    assert absent == 0.0
    assert 0.0 < aberrant < 0.01
    assert sortie[50, 50] == carte[50, 50]
    # le point à -200 m est loin du raccord du puits : il reste tel quel
    assert abs(sortie[200, 200] - (carte[200, 200] - 200.0)) < 1e-9


def test_trou_entier():
    """Une grille entièrement sans LiDAR retombe sur le relief en place."""
    carte = _carte(64)
    lidar = np.full_like(carte, config.NODATA - 1.0)
    sortie, absent, _, _ = combler(lidar, carte, MAILLE)
    assert absent == 100.0
    assert np.abs(sortie - carte).max() < 1e-6


if __name__ == "__main__":
    for nom, essai in sorted(globals().items()):
        if nom.startswith("test_"):
            essai()
            print(f"[ok] {nom}")
