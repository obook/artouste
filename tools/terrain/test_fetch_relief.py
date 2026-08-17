"""
test_fetch_relief.py
Vérifications du format de tuile de relief et du calage de sa grille. Rien n'est
téléchargé : ces deux points se cassent en silence, l'un en dégradant les
altitudes, l'autre en décalant les tuiles d'un pas sans que rien ne proteste.

  python3 -m terrain.test_fetch_relief

Auteur : O. Booklage
Licence : GPL v2
"""

import tempfile
import os

import numpy as np

from terrain.fetch_relief import (bornes_noeuds, decoder_tuile, ecrire_bloc,
                                  encoder_tuile)


CALAGE = {
    "largeur_m": 18432.0,
    "hauteur_m": 18432.0,
    "lon_min": 0.0,
    "lon_max": 0.25,
    "lat_min": 42.8,
    "lat_max": 43.0,
    "depart": None,
}


def test_aller_retour():
    """Une tuile de montagne, 1300 m de dénivelé : la quantification doit rester
       sous 3 cm, l'argument même du choix de 16 bits par point."""
    y, x = np.mgrid[0:512, 0:512]
    altitudes = 1000.0 + 1300.0 * (x / 511.0) * (y / 511.0)
    relues, pas = decoder_tuile(encoder_tuile(altitudes, 2.1898, 2.3112))
    assert pas == (np.float32(2.1898), np.float32(2.3112)) or (
        abs(pas[0] - 2.1898) < 1e-4 and abs(pas[1] - 2.3112) < 1e-4)
    assert np.abs(relues - altitudes).max() < 0.03, np.abs(relues - altitudes).max()


def test_tuile_plate():
    """Une tuile plate a une étendue nulle : elle ne doit pas diviser par zéro
       ni rendre autre chose que son altitude."""
    altitudes = np.full((512, 512), 37.5)
    relues, _ = decoder_tuile(encoder_tuile(altitudes, 2.0, 2.0))
    assert np.abs(relues - 37.5).max() < 1e-6


def test_grille_sans_recouvrement():
    """Les tuiles ne se recouvrent pas et ne laissent pas de trou : le dernier
       noeud d'un bloc doit tomber un pas exactement avant le premier du bloc
       suivant. Un pas d'écart ici décale toute la carte."""
    # Tuile NON carrée au sol : c'est le cas qui casse si un axe est oublié.
    tuile_x, tuile_z = 512 * 2.1898, 512 * 2.3112
    gauche = bornes_noeuds(CALAGE, tuile_x, tuile_z, 0, 0, 2, 2, 512)
    droite = bornes_noeuds(CALAGE, tuile_x, tuile_z, 2, 0, 2, 2, 512)
    pas_lon = (gauche[1] - gauche[0]) / (2 * 512 - 1)
    assert abs((droite[0] - gauche[1]) - pas_lon) < 1e-9 * pas_lon + 1e-12

    bas = bornes_noeuds(CALAGE, tuile_x, tuile_z, 0, 2, 2, 2, 512)
    pas_lat = (gauche[3] - gauche[2]) / (2 * 512 - 1)
    assert abs((gauche[2] - bas[3]) - pas_lat) < 1e-9 * pas_lat + 1e-12


def test_tuile_vide_non_ecrite():
    """Une tuile entièrement hors couverture LiDAR n'est pas écrite : le moteur
       y garde le relief d'ensemble de la carte."""
    altitudes = np.zeros((16, 32))
    manquant = np.zeros((16, 32), dtype=bool)
    manquant[:, 16:] = True  # la tuile de droite est vide
    with tempfile.TemporaryDirectory() as sortie:
        assert ecrire_bloc(sortie, altitudes, manquant, 0, 0, 2, 1, 16, 2.0, 2.0) == 1
        assert os.path.exists(os.path.join(sortie, "0", "0.r16"))
        assert not os.path.exists(os.path.join(sortie, "0", "1.r16"))


if __name__ == "__main__":
    for nom, essai in sorted(globals().items()):
        if nom.startswith("test_"):
            essai()
            print(f"[ok] {nom}")
