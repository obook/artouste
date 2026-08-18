#!/usr/bin/env python3
"""
Nom du fichier : relief_tuiles.py
Description : Format des tuiles de relief, grille, index et écriture d'un bloc.
Auteur : O. Booklage
Date : Août 2026
Licence : GPL v2
"""

import math
import os
import struct
from pathlib import Path

import numpy as np
from PIL import Image

from terrain.meta import read_meta

MAGIC = b"ARTR"
VERSION = 2

# Un point du laser qui plonge de plus de 300 m sous le relief en place ne
# mesure pas le sol : 421 points sur ossau, jusqu'à -796 m sur un versant à
# 2400 m. Même garde-fou que relief_lidar.py (CHUTE_ABERRANTE_M).
CHUTE_ABERRANTE_M = 300.0
# v1 : un seul pas, isotrope. v2 : un pas PAR AXE, la maille de la carte n'ayant
# pas la même finesse en x et en z. Le moteur lit les deux.
EN_TETE_V1 = "<4sHHfff"
EN_TETE_V2 = "<4sHHffff"
EN_TETE_OCTETS = struct.calcsize(EN_TETE_V2)

def encoder_tuile(altitudes, pas_x, pas_z):
    """Encode une tuile carrée d'altitudes en mètres. Le minimum et l'étendue
       sont ceux de CETTE tuile, ce qui donne la quantification la plus fine que
       16 bits permettent sur son propre dénivelé."""
    cote = altitudes.shape[0]
    mini = float(altitudes.min())
    # Une tuile parfaitement plate donnerait une étendue nulle et une division
    # par zéro ; le plancher ne coûte rien puisque tous les niveaux valent alors 0.
    etendue = max(float(altitudes.max()) - mini, 1e-3)
    niveaux = np.round((altitudes - mini) / etendue * 65535.0).astype("<u2")
    return (struct.pack(EN_TETE_V2, MAGIC, VERSION, cote, pas_x, pas_z, mini, etendue) +
            niveaux.tobytes())


def decoder_tuile(octets):
    """Décode une tuile, en mètres. Sert aux vérifications et aux outils ; le
       moteur fait la même chose en C++."""
    version = struct.unpack_from("<H", octets, 4)[0]
    if version == 1:
        magie, _, cote, pas_x, mini, etendue = struct.unpack_from(EN_TETE_V1, octets)
        pas_z = pas_x
        en_tete = struct.calcsize(EN_TETE_V1)
    else:
        magie, _, cote, pas_x, pas_z, mini, etendue = struct.unpack_from(EN_TETE_V2, octets)
        en_tete = struct.calcsize(EN_TETE_V2)
    if magie != MAGIC:
        raise ValueError(f"tuile de relief inattendue : {magie!r}")
    if version not in (1, 2):
        raise ValueError(f"version de tuile inconnue : {version}")
    niveaux = np.frombuffer(octets, dtype="<u2", offset=en_tete,
                            count=cote * cote).reshape(cote, cote)
    return mini + niveaux.astype(np.float64) / 65535.0 * etendue, (pas_x, pas_z)


class ReliefCarte:
    """Le relief d'ensemble de la carte, tel que le jeu le charge : heightmap.png
       en 16 bits, étalé de 0 à elev_max. Il sert à boucher les trous du LiDAR,
       pour que la fenêtre fine ne creuse jamais un puits là où le service n'a
       pas de donnée."""

    def __init__(self, dossier_carte):
        meta = read_meta(Path(dossier_carte) / "terrain.txt")
        niveaux = np.asarray(Image.open(Path(dossier_carte) / "heightmap.png"))
        self.altitudes = niveaux.astype(np.float64) / 65535.0 * float(meta["elev_max"])
        self.lon_min, self.lon_max = float(meta["lon_min"]), float(meta["lon_max"])
        self.lat_min, self.lat_max = float(meta["lat_min"]), float(meta["lat_max"])

    def echantillonner(self, lons, lats):
        """Altitudes aux points donnés, au plus proche. Au plus proche suffit :
           on ne s'en sert que dans les trous, et la maille d'ensemble est de
           toute façon cent fois plus grossière que ce qu'on fabrique."""
        rangees, colonnes = self.altitudes.shape
        i = np.round((lons - self.lon_min) / (self.lon_max - self.lon_min) * (colonnes - 1))
        j = np.round((self.lat_max - lats) / (self.lat_max - self.lat_min) * (rangees - 1))
        i = np.clip(i, 0, colonnes - 1).astype(np.intp)
        j = np.clip(j, 0, rangees - 1).astype(np.intp)
        return self.altitudes[j[:, None], i[None, :]]


def ecrire_index(sortie, carte, calage, meta, tuile_points, pas_x, pas_z, colonnes, rangees):
    """Index à la racine du jeu, dans les mêmes termes que celui des tuiles
       d'image : la grille est ancrée sur le coin nord-ouest de la carte, en
       coordonnées monde (X est, Z sud), dont l'origine est le centre de
       l'emprise décalé par origin_x / origin_z."""
    coin_x = float(meta.get("origin_x", 0.0)) - calage["largeur_m"] / 2.0
    coin_z = float(meta.get("origin_z", 0.0)) - calage["hauteur_m"] / 2.0
    with open(os.path.join(sortie, "index.txt"), "w", encoding="utf-8") as f:
        f.write(f"# Tuiles de relief Artouste - {carte}\n")
        f.write("# Grille ancrée sur le coin nord-ouest de la tuile (0, 0), en\n")
        f.write("# coordonnées monde (X est, Z sud). Une tuile par fichier :\n")
        f.write("# <rangée>/<colonne>.r16, 16 bits par point (voir fetch_relief.py).\n")
        f.write(f"tuile_points {tuile_points}\n")
        f.write(f"pas_x {pas_x}\n")
        f.write(f"pas_z {pas_z}\n")
        f.write(f"colonnes {colonnes}\n")
        f.write(f"rangees {rangees}\n")
        f.write(f"coin_x {coin_x:.2f}\n")
        f.write(f"coin_z {coin_z:.2f}\n")


def grille_relief(calage, tuile_points, pas_x, pas_z):
    """Nombre de tuiles couvrant l'emprise, avec un pas PAR AXE : une tuile n'est
       plus carrée au sol."""
    tuile_x = tuile_points * pas_x
    tuile_z = tuile_points * pas_z
    return (math.ceil(calage["largeur_m"] / tuile_x),
            math.ceil(calage["hauteur_m"] / tuile_z),
            tuile_x, tuile_z)


def bbox_relief(calage, tuile_x, tuile_z, col0, rangee0, n_col, n_rangee):
    """Emprise géographique d'un bloc, tuiles non carrées. Même ancrage que
       bbox_bloc : coin nord-ouest, rangée 0 au nord."""
    deg_lon = (calage["lon_max"] - calage["lon_min"]) / calage["largeur_m"]
    deg_lat = (calage["lat_max"] - calage["lat_min"]) / calage["hauteur_m"]
    lon_lo = calage["lon_min"] + col0 * tuile_x * deg_lon
    lon_hi = lon_lo + n_col * tuile_x * deg_lon
    lat_hi = calage["lat_max"] - rangee0 * tuile_z * deg_lat
    lat_lo = lat_hi - n_rangee * tuile_z * deg_lat
    return (lon_lo, lon_hi, lat_lo, lat_hi)


def bornes_noeuds(calage, tuile_x, tuile_z, col0, rangee0, n_col, n_rangee, tuile_points):
    """Position des noeuds EXTRÊMES d'un bloc, en degrés, au format attendu par
       grille_altitudes : (lon_min, lon_max, lat_min, lat_max).

       Le premier noeud d'une tuile tombe sur son coin nord-ouest, et le dernier
       un pas AVANT le coin suivant : les tuiles ne se recouvrent pas."""
    lon_lo, lon_hi, lat_lo, lat_hi = bbox_relief(calage, tuile_x, tuile_z, col0, rangee0,
                                                 n_col, n_rangee)
    pas_lon = (lon_hi - lon_lo) / (n_col * tuile_points)
    pas_lat = (lat_hi - lat_lo) / (n_rangee * tuile_points)
    return (lon_lo, lon_hi - pas_lon, lat_lo + pas_lat, lat_hi)


def ecrire_bloc(sortie, altitudes, manquant, col0, rangee0, n_col, n_rangee,
                tuile_points, pas_x, pas_z):
    """Découpe un bloc en tuiles et les écrit. Renvoie le nombre de tuiles
       écrites : une tuile entièrement hors couverture LiDAR est laissée
       absente, le moteur y gardant le relief d'ensemble."""
    ecrites = 0
    for r in range(n_rangee):
        for c in range(n_col):
            tranche = (slice(r * tuile_points, (r + 1) * tuile_points),
                       slice(c * tuile_points, (c + 1) * tuile_points))
            if manquant[tranche].all():
                continue
            dossier = os.path.join(sortie, str(rangee0 + r))
            os.makedirs(dossier, exist_ok=True)
            chemin = os.path.join(dossier, f"{col0 + c}.r16")
            with open(chemin, "wb") as f:
                f.write(encoder_tuile(altitudes[tranche], pas_x, pas_z))
            ecrites += 1
    return ecrites
