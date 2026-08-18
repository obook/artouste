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
from terrain.fetch_tuiles import (bloc_complet, lire_calage,
                                  lire_points_poser, marquer_bloc,
                                  marquer_inacheve, retirer_marque_inacheve,
                                  zone_autour, bloc_dans_zones)
from terrain.meta import read_meta

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from lidar.services import COUCHE_MNT, grille_altitudes

from terrain.relief_tuiles import (CHUTE_ABERRANTE_M, EN_TETE_OCTETS, MAGIC,
                                   VERSION)
from terrain.relief_tuiles import (ReliefCarte, bbox_relief, bornes_noeuds,
                                   decoder_tuile, ecrire_bloc, ecrire_index,
                                   encoder_tuile, grille_relief)

def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("carte", help="nom du dossier sous assets/terrain")
    p.add_argument("sortie", help="dossier où écrire les tuiles de relief")
    # Pas PAR AXE : il doit valoir dx/k et dz/k de la carte, k multiple de
    # PAS_ANNEAU (4), pour que le noyau ET l'anneau de la fenêtre s'emboîtent
    # dans la maille du maillage d'ensemble. Sans cela la fenêtre redessine la
    # surface au lieu de la reproduire, et sa frontière se voit en vol.
    # --pas-m reste accepté et pose les deux à la même valeur.
    p.add_argument("--pas-m", type=float, default=None)
    p.add_argument("--pas-x", type=float, default=None)
    p.add_argument("--pas-z", type=float, default=None)
    p.add_argument("--tuile-points", type=int, default=512)
    p.add_argument("--bloc-tuiles", type=int, default=2)
    p.add_argument("--surech", type=int, default=2)
    p.add_argument("--autour", nargs=3, type=float, action="append", default=[],
                   metavar=("LON", "LAT", "RAYON_KM"))
    p.add_argument("--autour-helipads", type=float, default=0.0, metavar="RAYON_KM")
    p.add_argument("--reprendre", action="store_true")
    args = p.parse_args()
    if args.pas_x is None:
        args.pas_x = args.pas_m if args.pas_m is not None else 2.0
    if args.pas_z is None:
        args.pas_z = args.pas_m if args.pas_m is not None else args.pas_x

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
    colonnes, rangees, tuile_x, tuile_z = grille_relief(calage, args.tuile_points,
                                                        args.pas_x, args.pas_z)

    os.makedirs(args.sortie, exist_ok=True)
    ecrire_index(args.sortie, args.carte, calage, meta, args.tuile_points,
                 args.pas_x, args.pas_z, colonnes, rangees)
    marquer_inacheve(args.sortie, args.pas_x, colonnes * rangees)

    relief_carte = ReliefCarte(dossier_carte)

    zones = [zone_autour(lon, lat, rayon) for lon, lat, rayon in args.autour]
    if args.autour_helipads > 0.0:
        for lon, lat in lire_points_poser(dossier_carte, calage):
            zones.append(zone_autour(lon, lat, args.autour_helipads))

    blocs_x = math.ceil(colonnes / args.bloc_tuiles)
    blocs_y = math.ceil(rangees / args.bloc_tuiles)
    octets = colonnes * rangees * args.tuile_points * args.tuile_points * 2
    print(f"[relief] {args.carte} : {colonnes} x {rangees} = {colonnes * rangees} tuiles "
          f"de {args.tuile_points} points à {args.pas_x:.4f} x {args.pas_z:.4f} m "
          f"({tuile_x:.0f} x {tuile_z:.0f} m au sol), "
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

            lon_lo, lon_hi, lat_lo, lat_hi = bbox_relief(calage, tuile_x, tuile_z, col0, rangee0,
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

            bornes = bornes_noeuds(calage, tuile_x, tuile_z, col0, rangee0, n_col, n_rangee,
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
                            n_col, n_rangee, args.tuile_points, args.pas_x, args.pas_z)
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
