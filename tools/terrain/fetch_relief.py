"""
fetch_relief.py
Récupère auprès de l'IGN le MNT LiDAR HD d'une carte et le découpe en tuiles de
relief, le jeu fin que le simulateur charge par morceaux autour de l'appareil.
C'est au relief ce que fetch_tuiles.py est à l'orthophoto, et le découpage suit
exactement la même grille : mêmes blocs, même index, même témoin d'inachèvement.

Pourquoi. Le maillage d'une carte est figé au chargement et plafonné par
relief_sommets_max : sur 18 km cela fait 17,5 m de maille. La même dépense en
sommets, autour de l'appareil, donne 2 m.

Le relief pèse peu à côté de l'image : deux octets par point contre trois par
pixel, et 2 m de maille contre 0,25. Bigorre demande 186 Mo.

Le MNT et non le MNS : le sol NU. Le MNS mettrait les toits et les cimes dans le
relief, et l'on se poserait dessus.

Format d'une tuile, <rangée>/<colonne>.r16 :

  en-tête de 20 octets, tout en petit-boutien
    4s  "ARTR"
    H   version (1)
    H   côté en points (512)
    f   pas en mètres (2.0)
    f   altitude minimale de la tuile, en mètres
    f   étendue d'altitude de la tuile, en mètres
  puis côté x côté entiers 16 bits non signés, rangée 0 au NORD,
  altitude = minimum + niveau / 65535 x étendue

Le couple (minimum, étendue) est propre à chaque tuile : sur 1300 m de dénivelé
la quantification tombe à 2 cm.

Les points ne se recouvrent pas d'une tuile à l'autre : le point global (I, J)
est dans la tuile (I / 512, J / 512) à l'indice (I % 512, J % 512). La fenêtre du
moteur est une grille continue qui les lit tous, sans couture à recoudre.

Une tuile entièrement hors couverture LiDAR n'est pas écrite ; le moteur y garde
le relief d'ensemble. Les trous partiels sont bouchés avec ce même relief.

Usage :
  python3 -m terrain.fetch_relief <carte> <dossier-sortie> [options]

  --pas-m M         maille en mètres (défaut 2.0)
  --tuile-points N  côté d'une tuile en points (défaut 512, soit 1024 m à 2 m)
  --bloc-tuiles N   tuiles par côté de bloc (défaut 2)
  --surech N        suréchantillonnage avant moyenne (défaut 2 : le MNT LiDAR
                    est natif à 1 m, deux pixels par point le moyennent
                    exactement)
  --autour LON LAT RAYON_KM      ne récupère que les blocs proches de ce point
  --autour-helipads RAYON_KM     idem autour des hélisurfaces et du départ
  --reprendre       saute les blocs déjà faits

Exemple, en tâche de fond parce que c'est long :

  python3 -m terrain.fetch_relief bigorre /media/disque/bigorre.relief --reprendre

Le dossier de sortie est un FRÈRE de celui des tuiles d'image, pas un
sous-dossier : le moteur prend tout sous-dossier portant un index.txt pour un
niveau d'image de plus (voir src/render/tuiles/Pyramide.cpp).

Données : IGN Géoplateforme (LiDAR HD), Licence Ouverte Etalab 2.0.

Auteur : O. Booklage
Licence : GPL v2
"""

import argparse
import math
import os
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

from terrain import config
from terrain.fetch_tuiles import (bbox_bloc, bloc_complet, grille, lire_calage,
                                  lire_points_poser, marquer_bloc,
                                  marquer_inacheve, retirer_marque_inacheve,
                                  zone_autour, bloc_dans_zones)
from terrain.meta import read_meta

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from lidar.services import COUCHE_MNT, grille_altitudes

MAGIC = b"ARTR"
VERSION = 1

# Un point du laser qui plonge de plus de 300 m sous le relief en place ne
# mesure pas le sol : 421 points sur ossau, jusqu'à -796 m sur un versant à
# 2400 m. Même garde-fou que relief_lidar.py (CHUTE_ABERRANTE_M).
CHUTE_ABERRANTE_M = 300.0
EN_TETE = "<4sHHfff"
EN_TETE_OCTETS = struct.calcsize(EN_TETE)


def encoder_tuile(altitudes, pas_m):
    """Encode une tuile carrée d'altitudes en mètres. Le minimum et l'étendue
       sont ceux de CETTE tuile, ce qui donne la quantification la plus fine que
       16 bits permettent sur son propre dénivelé."""
    cote = altitudes.shape[0]
    mini = float(altitudes.min())
    # Une tuile parfaitement plate donnerait une étendue nulle et une division
    # par zéro ; le plancher ne coûte rien puisque tous les niveaux valent alors 0.
    etendue = max(float(altitudes.max()) - mini, 1e-3)
    niveaux = np.round((altitudes - mini) / etendue * 65535.0).astype("<u2")
    return struct.pack(EN_TETE, MAGIC, VERSION, cote, pas_m, mini, etendue) + niveaux.tobytes()


def decoder_tuile(octets):
    """Décode une tuile, en mètres. Sert aux vérifications et aux outils ; le
       moteur fait la même chose en C++."""
    magie, version, cote, pas_m, mini, etendue = struct.unpack_from(EN_TETE, octets)
    if magie != MAGIC:
        raise ValueError(f"tuile de relief inattendue : {magie!r}")
    if version != VERSION:
        raise ValueError(f"version de tuile inconnue : {version}")
    niveaux = np.frombuffer(octets, dtype="<u2", offset=EN_TETE_OCTETS,
                            count=cote * cote).reshape(cote, cote)
    return mini + niveaux.astype(np.float64) / 65535.0 * etendue, pas_m


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


def ecrire_index(sortie, carte, calage, meta, tuile_points, pas_m, colonnes, rangees):
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
        f.write(f"pas_m {pas_m}\n")
        f.write(f"colonnes {colonnes}\n")
        f.write(f"rangees {rangees}\n")
        f.write(f"coin_x {coin_x:.2f}\n")
        f.write(f"coin_z {coin_z:.2f}\n")


def bornes_noeuds(calage, tuile_m, col0, rangee0, n_col, n_rangee, tuile_points):
    """Position des noeuds EXTRÊMES d'un bloc, en degrés, au format attendu par
       grille_altitudes : (lon_min, lon_max, lat_min, lat_max).

       Le premier noeud d'une tuile tombe sur son coin nord-ouest, et le dernier
       un pas AVANT le coin suivant : les tuiles ne se recouvrent pas."""
    lon_lo, lon_hi, lat_lo, lat_hi = bbox_bloc(calage, tuile_m, col0, rangee0, n_col, n_rangee)
    pas_lon = (lon_hi - lon_lo) / (n_col * tuile_points)
    pas_lat = (lat_hi - lat_lo) / (n_rangee * tuile_points)
    return (lon_lo, lon_hi - pas_lon, lat_lo + pas_lat, lat_hi)


def ecrire_bloc(sortie, altitudes, manquant, col0, rangee0, n_col, n_rangee,
                tuile_points, pas_m):
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
                f.write(encoder_tuile(altitudes[tranche], pas_m))
            ecrites += 1
    return ecrites


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("carte", help="nom du dossier sous assets/terrain")
    p.add_argument("sortie", help="dossier où écrire les tuiles de relief")
    p.add_argument("--pas-m", type=float, default=2.0)
    p.add_argument("--tuile-points", type=int, default=512)
    p.add_argument("--bloc-tuiles", type=int, default=2)
    p.add_argument("--surech", type=int, default=2)
    p.add_argument("--autour", nargs=3, type=float, action="append", default=[],
                   metavar=("LON", "LAT", "RAYON_KM"))
    p.add_argument("--autour-helipads", type=float, default=0.0, metavar="RAYON_KM")
    p.add_argument("--reprendre", action="store_true")
    args = p.parse_args()

    dossier_carte = os.path.join(config.TERRAIN_ROOT, args.carte)
    terrain_txt = os.path.join(dossier_carte, "terrain.txt")
    if not os.path.exists(terrain_txt):
        raise SystemExit(f"carte inconnue : {terrain_txt}")

    points_bloc = args.bloc_tuiles * args.tuile_points * args.surech
    if points_bloc > config.WMS_MAX_PX:
        # grille_altitudes redécoupe de lui-même, mais un bloc plus grand que la
        # limite serveur ne gagne alors plus rien et occupe la mémoire pour rien.
        raise SystemExit(f"bloc de {points_bloc} px demandés : au-dessus de la limite "
                         f"serveur IGN ({config.WMS_MAX_PX} px). Baisser --bloc-tuiles "
                         f"ou --surech.")

    calage = lire_calage(terrain_txt)
    meta = read_meta(Path(terrain_txt))
    colonnes, rangees, tuile_m = grille(calage, args.tuile_points, args.pas_m)

    os.makedirs(args.sortie, exist_ok=True)
    ecrire_index(args.sortie, args.carte, calage, meta, args.tuile_points,
                 args.pas_m, colonnes, rangees)
    marquer_inacheve(args.sortie, args.pas_m, colonnes * rangees)

    relief_carte = ReliefCarte(dossier_carte)

    zones = [zone_autour(lon, lat, rayon) for lon, lat, rayon in args.autour]
    if args.autour_helipads > 0.0:
        for lon, lat in lire_points_poser(dossier_carte, calage):
            zones.append(zone_autour(lon, lat, args.autour_helipads))

    blocs_x = math.ceil(colonnes / args.bloc_tuiles)
    blocs_y = math.ceil(rangees / args.bloc_tuiles)
    octets = colonnes * rangees * args.tuile_points * args.tuile_points * 2
    print(f"[relief] {args.carte} : {colonnes} x {rangees} = {colonnes * rangees} tuiles "
          f"de {args.tuile_points} points à {args.pas_m} m ({tuile_m:.0f} m au sol), "
          f"{octets / 1e6:.0f} Mo au plus")
    print(f"[relief] {blocs_x * blocs_y} blocs de {args.bloc_tuiles} x {args.bloc_tuiles} "
          f"tuiles, suréchantillonnage x{args.surech}")
    sys.stdout.flush()

    faits = 0
    hors_zone = 0
    ecrites = 0
    for by in range(blocs_y):
        for bx in range(blocs_x):
            col0 = bx * args.bloc_tuiles
            rangee0 = by * args.bloc_tuiles
            n_col = min(args.bloc_tuiles, colonnes - col0)
            n_rangee = min(args.bloc_tuiles, rangees - rangee0)

            lon_lo, lon_hi, lat_lo, lat_hi = bbox_bloc(calage, tuile_m, col0, rangee0,
                                                       n_col, n_rangee)
            if not bloc_dans_zones(lon_lo, lon_hi, lat_lo, lat_hi, zones):
                hors_zone += 1
                continue

            faits += 1
            if args.reprendre and bloc_complet(args.sortie, col0, rangee0):
                print(f"[{faits}] bloc ({col0}, {rangee0}) déjà fait")
                continue

            nx = n_col * args.tuile_points
            nz = n_rangee * args.tuile_points
            print(f"[{faits}] bloc ({col0}, {rangee0}) {nx}x{nz} points, WMS...")
            sys.stdout.flush()

            bornes = bornes_noeuds(calage, tuile_m, col0, rangee0, n_col, n_rangee,
                                   args.tuile_points)
            altitudes = grille_altitudes(COUCHE_MNT, bornes, nx, nz,
                                         max(1, args.surech), journal=False)

            manquant = altitudes <= config.NODATA
            lons = np.linspace(bornes[0], bornes[1], nx)
            lats = np.linspace(bornes[3], bornes[2], nz)
            carte_alt = relief_carte.echantillonner(lons, lats)
            aberrant = ~manquant & (altitudes < carte_alt - CHUTE_ABERRANTE_M)
            if manquant.any() or aberrant.any():
                altitudes = np.where(manquant | aberrant, carte_alt, altitudes)

            n = ecrire_bloc(args.sortie, altitudes, manquant, col0, rangee0,
                            n_col, n_rangee, args.tuile_points, args.pas_m)
            ecrites += n
            marquer_bloc(args.sortie, col0, rangee0)
            trace_aberrant = f", {int(aberrant.sum())} point(s) aberrant(s) écarté(s)" \
                if aberrant.any() else ""
            print(f"Bloc ({col0}, {rangee0}) : {n} tuiles écrites, "
                  f"{float(manquant.mean()) * 100.0:.0f} % sans donnée LiDAR"
                  f"{trace_aberrant}.")
            sys.stdout.flush()

    retirer_marque_inacheve(args.sortie)
    if hors_zone > 0:
        print(f"[relief] {hors_zone} blocs hors des zones demandées, non téléchargés")
    print(f"[relief] terminé : {faits} blocs traités, {ecrites} tuiles, {args.sortie}")


if __name__ == "__main__":
    main()
