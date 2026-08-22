"""
test_depart.py
Choix du point de départ d'une carte (relief.find_flat_start). Rien n'est
téléchargé : on lui donne une grille d'altitudes fabriquée à la main.

Ce que ces essais protègent : la recherche ne compare pas QUE la planéité. Sans
terme de distance elle part au bout de sa fenêtre pour un gain dérisoire, et le
départ finit à un kilomètre de l'hélipad qu'il est censé désigner (mesuré sur
arcachon : 800 m payés pour 0,20 m de dénivelé).

  python3 -m terrain.test_depart

Auteur : O. Booklage
Licence : GPL v2
"""

import numpy as np

from terrain import config
from terrain.relief import find_flat_start


COTE = 64
LARGEUR_M = 640.0   # mailles de ~10 m
HAUTEUR_M = 640.0


def _preparer():
    """Cale les variables de module que find_flat_start lit, et vise le centre."""
    config.COLS = config.ROWS = COTE
    config.LON_MIN, config.LON_MAX = 0.0, 1.0
    config.LAT_MIN, config.LAT_MAX = 0.0, 1.0
    # Cible au centre exact de la grille.
    config.START_LON = 0.5
    config.START_LAT = 0.5


def _cellule(start_x, start_z):
    """Cellule correspondant au couple renvoyé par find_flat_start."""
    col = round((start_x / LARGEUR_M + 0.5) * (COTE - 1))
    row = round((start_z / HAUTEUR_M + 0.5) * (COTE - 1))
    return col, row


def test_ne_voyage_pas_pour_du_bruit():
    """Terrain plat partout, avec un creux d'un centimètre au loin : le départ
       doit rester près de la cible, pas courir après le centimètre."""
    _preparer()
    grille = np.full((COTE, COTE), 100.0)
    grille[4, 4] = 99.99  # aussi loin que la fenêtre le permet, gain dérisoire
    col, row = _cellule(*find_flat_start(grille, LARGEUR_M, HAUTEUR_M))
    centre = (COTE - 1) / 2
    assert abs(col - centre) <= 2 and abs(row - centre) <= 2, (col, row)


def test_va_chercher_un_vrai_replat():
    """Cible sur un versant, replat franc à vingt mailles : là, le déplacement
       vaut son prix et la recherche doit y aller."""
    _preparer()
    # Pente est-ouest de 5 m par maille : tout voisinage y accuse 10 m.
    grille = np.tile(np.arange(COTE, dtype=float) * 5.0, (COTE, 1))
    # Replat parfaitement horizontal au sud-est, à une douzaine de mailles de la
    # cible : il ne la recouvre pas, il faut aller le chercher.
    grille[44:52, 44:52] = 500.0
    col, row = _cellule(*find_flat_start(grille, LARGEUR_M, HAUTEUR_M))
    assert 45 <= col <= 50 and 45 <= row <= 50, (col, row)


def test_la_maille_rectangulaire_compte_en_metres():
    """La distance se mesure en mètres, pas en cellules : sur une carte deux fois
       plus haute que large, un pas vers le nord coûte deux fois un pas vers
       l'est. Deux replats identiques à dix cellules, l'un à l'ouest, l'autre au
       nord : c'est celui de l'ouest, plus proche en mètres, qui doit gagner."""
    _preparer()
    # Pente est-ouest : partout ailleurs, le voisinage accuse 10 m de dénivelé.
    grille = np.tile(np.arange(COTE, dtype=float) * 5.0, (COTE, 1))
    centre = (COTE - 1) // 2
    grille[centre - 1:centre + 2, centre - 11:centre - 8] = 300.0  # replat à l'ouest
    grille[centre - 11:centre - 8, centre - 1:centre + 2] = 300.0  # replat au nord
    col, row = _cellule(*find_flat_start(grille, LARGEUR_M, 2.0 * HAUTEUR_M))
    assert abs(row - centre) <= 1, (col, row)
    assert abs(col - (centre - 10)) <= 1, (col, row)


if __name__ == "__main__":
    for nom, essai in sorted(globals().items()):
        if nom.startswith("test_"):
            essai()
            print(f"[ok] {nom}")
