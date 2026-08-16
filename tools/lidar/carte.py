"""
carte.py
Ce qui relie un modèle LiDAR à la carte qui l'accueille : lecture du relief de
la carte, recalage altimétrique entre les deux jeux de données, et raccord des
bords.

Auteur : O. Booklage
Licence : GPL v2
"""

from pathlib import Path

import numpy as np
from PIL import Image

from terrain import config
from terrain.meta import read_meta

# Conversion degrés <-> mètres. MÊME convention que fetch_terrain.py
# (équirectangulaire locale), pour que le modèle partage l'échelle du moteur.
M_PAR_DEG_LAT = 111320.0

PORTEE_RECALAGE = 40.0  # largeur du lissage de l'écart entre LiDAR et carte (m)
MARGE_FONDU = 20.0      # largeur du raccord au relief de la carte (m)
# Marge du sol du modèle au-dessus du relief de la carte. Elle doit passer
# au-dessus de la triangulation du terrain, dont les mailles coupent la corde
# sous l'interpolation bilinéaire que l'on échantillonne ici.
EPAISSEUR_SOL = 0.40


def sol_de_la_carte(carte, lons, lats):
    """Altitude du relief de la carte aux points donnés, par interpolation
       bilinéaire de heightmap.png.

       C'est vers CE sol que les bords du modèle doivent fondre, et non vers le
       MNT : c'est lui que le moteur dessine, et il en diffère de plusieurs
       mètres, sa maille faisant 17,5 m."""
    dossier = Path(config.TERRAIN_ROOT) / carte
    meta = read_meta(dossier / "terrain.txt")
    cols, rows = int(meta["cols"]), int(meta["rows"])
    elev_min, elev_max = float(meta["elev_min"]), float(meta["elev_max"])
    lon_min, lon_max = float(meta["lon_min"]), float(meta["lon_max"])
    lat_min, lat_max = float(meta["lat_min"]), float(meta["lat_max"])

    niveaux = np.asarray(Image.open(dossier / "heightmap.png"), dtype=np.float64)
    if niveaux.shape != (rows, cols):
        raise RuntimeError(f"{carte} : heightmap {niveaux.shape} pour une grille "
                           f"{rows}x{cols} annoncée dans terrain.txt")
    altitudes = elev_min + niveaux / 65535.0 * (elev_max - elev_min)

    colonne = np.clip((lons - lon_min) / (lon_max - lon_min) * (cols - 1), 0.0, cols - 1.001)
    rangee = np.clip((lat_max - lats) / (lat_max - lat_min) * (rows - 1), 0.0, rows - 1.001)
    c0, r0 = np.floor(colonne).astype(int), np.floor(rangee).astype(int)
    fc, fr = colonne - c0, rangee - r0
    return (altitudes[r0, c0] * (1.0 - fc) * (1.0 - fr)
            + altitudes[r0, c0 + 1] * fc * (1.0 - fr)
            + altitudes[r0 + 1, c0] * (1.0 - fc) * fr
            + altitudes[r0 + 1, c0 + 1] * fc * fr)


def flou_boite(valeurs, cote):
    """Moyenne glissante sur un carré cote x cote, bords prolongés. Sert à ne
       garder que la basse fréquence d'un écart, sans dépendance nouvelle."""
    rayon = max(1, cote // 2)
    borde = np.pad(valeurs, rayon, mode="edge")
    cumul = np.zeros((borde.shape[0] + 1, borde.shape[1] + 1), dtype=np.float64)
    cumul[1:, 1:] = borde.cumsum(0).cumsum(1)
    large = 2 * rayon + 1
    somme = (cumul[large:, large:] - cumul[:-large, large:]
             - cumul[large:, :-large] + cumul[:-large, :-large])
    return somme / (large * large)


def recaler_sur_la_carte(mns, mnt, sol):
    """Ramène la surface LiDAR sur le référentiel altimétrique de la carte.

       Les deux jeux ne s'accordent pas sur ce sommet : le RGE ALTI de la carte
       s'y tient environ 4 m au-dessus du sol nu du LiDAR (mesuré, avec des
       écarts de -27 à +28 m sur les falaises que sa maille de 17,5 m coupe).
       On ajoute donc au MNS l'écart BASSE FRÉQUENCE entre le sol de la carte et
       le sol nu du LiDAR : le modèle s'assoit sur la carte tout en gardant son
       relief fin et la hauteur vraie de ses bâtiments.

       Piège écarté au passage. Poser une SURHAUTEUR (mns - mnt) sur le relief de
       la carte paraît plus simple et supprime tout risque de traversée, mais
       fabrique des piliers : sous une terrasse en bord de falaise, le sol nu du
       LiDAR plonge, la surhauteur y atteint 47 m, et cette hauteur se dresse
       alors sur une pente que la carte a lissée. La surface absolue, elle, reste
       fidèle."""
    ecart = flou_boite(sol - mnt, int(round(PORTEE_RECALAGE)))
    return mns + ecart, ecart


def fondu_des_bords(nx, nz, pas, marge):
    """Poids de 0 au bord de l'emprise à 1 au-delà de la marge, adouci aux deux
       extrémités (courbe en S), pour raccorder le modèle au relief de la carte
       sans marche ni cassure."""
    x = np.minimum(np.arange(nx), nx - 1 - np.arange(nx)) * pas
    z = np.minimum(np.arange(nz), nz - 1 - np.arange(nz)) * pas
    distance = np.minimum(x[None, :], z[:, None])
    t = np.clip(distance / max(marge, 1e-6), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)
