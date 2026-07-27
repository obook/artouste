"""
fetch_tuiles.py
Récupère auprès de l'IGN l'orthophoto FINE d'une carte et la découpe en tuiles
BC7, le jeu de détail que le simulateur charge par morceaux autour de l'appareil
(voir src/render/tuiles/Pyramide.hpp).

Pourquoi un script à part de fetch_terrain.py. L'orthophoto d'ensemble d'une
carte tient dans une image : quelques milliers de pixels de côté, quelques
mégaoctets. Le jeu de détail, lui, est d'un autre ordre : le bassin d'Arcachon à
1,5 m/px fait 780 mégapixels, et personne ne tient cette mosaïque en mémoire pour
la découper ensuite. On travaille donc par BLOCS : une requête WMS, un découpage,
le bloc est jeté, on passe au suivant. La mémoire occupée ne dépend plus de la
carte, et une interruption ne coûte que le bloc en cours (option --reprendre).

Usage :
  python3 -m terrain.fetch_tuiles <carte> <dossier-sortie> [options]

  --m-par-pixel M   finesse cible (défaut 0.75 ; l'IGN photographie à 0,20 m/px
                    sur presque toute la France, en deçà il n'y a rien à gagner)
  --tuile-px N      côté d'une tuile (défaut 512, comme le moteur)
  --bloc-tuiles N   tuiles par côté de bloc (défaut 9, soit 4608 px, sous la
                    limite serveur IGN de ~5010 px par requête)
  --autour LON LAT RAYON_KM
                    ne récupère que les blocs proches de ce point ; répétable.
                    Sans cette option, toute l'emprise de la carte est couverte.
  --autour-helipads RAYON_KM
                    idem autour de chaque hélisurface de la carte et de son
                    point de départ (helipads.txt et terrain.txt)
  --reprendre       saute les tuiles déjà écrites
  --outil CHEMIN    exécutable orthotuiles (défaut build/bin/orthotuiles)

Exemples, en tâche de fond parce que c'est long (des heures sur une grande
carte) :

  # Niveau large, toute l'emprise
  python3 -m terrain.fetch_tuiles ossau /media/disque/ossau --m-par-pixel 0.75

  # Niveau serré, seulement là où l'on se pose : un sous-dossier du précédent,
  # que le moteur reconnaît comme un second niveau (voir Pyramide.hpp)
  python3 -m terrain.fetch_tuiles ossau /media/disque/ossau/serre \
          --m-par-pixel 0.20 --autour-helipads 0.8

Ordre de grandeur du disque, à 512 px par tuile et 1,33 octet par pixel (BC7 et
ses niveaux de réduction) : 0,9 Go pour Ossau à 0,75 m/px, 1,0 Go pour le bassin
d'Arcachon à 1,5 m/px, 0,8 Go pour Paris à 0,5 m/px.

Auteur : O. Booklage
Licence : GPL v2
"""

import argparse
import math
import os
import subprocess
import sys
import tempfile

from terrain import config
from terrain.ortho import request_map


def lire_calage(chemin):
    """Lit le terrain.txt d'une carte : emprise au sol, bornes géographiques et
       décalage d'origine. On le lit plutôt que d'interroger zones.py, pour que
       les cartes recadrées (origin_x / origin_z) marchent sans cas
       particulier."""
    valeurs = {}
    with open(chemin, encoding="utf-8") as f:
        for ligne in f:
            ligne = ligne.strip()
            if not ligne or ligne.startswith("#"):
                continue
            morceaux = ligne.split(None, 1)
            if len(morceaux) == 2:
                valeurs[morceaux[0]] = morceaux[1].strip()

    manquantes = [c for c in ("width_m", "height_m", "lon_min", "lon_max",
                              "lat_min", "lat_max") if c not in valeurs]
    if manquantes:
        raise SystemExit(f"terrain.txt incomplet, clés manquantes : {', '.join(manquantes)}")

    return {
        "largeur_m": float(valeurs["width_m"]),
        "hauteur_m": float(valeurs["height_m"]),
        "lon_min": float(valeurs["lon_min"]),
        "lon_max": float(valeurs["lon_max"]),
        "lat_min": float(valeurs["lat_min"]),
        "lat_max": float(valeurs["lat_max"]),
        # Point de départ, en degrés : il est donné en coordonnées monde, dont
        # l'origine est le centre de l'emprise (décalé par origin_x / origin_z
        # sur une carte recadrée), et l'axe Z pointe vers le sud.
        "depart": _depart_en_degres(valeurs),
    }


def _depart_en_degres(valeurs):
    """Convertit start_x / start_z en (lon, lat), ou None si la carte n'en donne
       pas."""
    if "start_x" not in valeurs or "start_z" not in valeurs:
        return None
    largeur = float(valeurs["width_m"])
    hauteur = float(valeurs["height_m"])
    origine_x = float(valeurs.get("origin_x", 0.0))
    origine_z = float(valeurs.get("origin_z", 0.0))
    x = float(valeurs["start_x"])
    z = float(valeurs["start_z"])
    part_col = (x - origine_x) / largeur + 0.5
    part_rangee = (z - origine_z) / hauteur + 0.5
    lon_min, lon_max = float(valeurs["lon_min"]), float(valeurs["lon_max"])
    lat_min, lat_max = float(valeurs["lat_min"]), float(valeurs["lat_max"])
    return (lon_min + part_col * (lon_max - lon_min),
            lat_max - part_rangee * (lat_max - lat_min))


def grille(calage, tuile_px, m_par_pixel):
    """Nombre de tuiles couvrant l'emprise. MÊME calcul que l'outil C++
       (src/tools/orthotuiles.cpp) : les deux doivent tomber d'accord, sans quoi
       les blocs ne se rangeraient pas aux bons indices."""
    tuile_m = tuile_px * m_par_pixel
    return (math.ceil(calage["largeur_m"] / tuile_m),
            math.ceil(calage["hauteur_m"] / tuile_m),
            tuile_m)


def bbox_bloc(calage, tuile_m, col0, rangee0, n_col, n_rangee):
    """Emprise géographique d'un bloc de tuiles. La grille est ancrée sur le coin
       nord-ouest de la carte, et le passage des mètres aux degrés est linéaire
       sur chaque axe, comme partout ailleurs dans le projet.

       Le dernier bloc d'une rangée dépasse l'emprise de la carte (la grille la
       couvre entièrement, à la tuile près) : on demande alors des degrés
       au-delà des bornes, ce qui est voulu. Le service y répond par de vraies
       images, et les tuiles de bord ont ainsi du paysage plutôt qu'un vide."""
    deg_par_m_lon = (calage["lon_max"] - calage["lon_min"]) / calage["largeur_m"]
    deg_par_m_lat = (calage["lat_max"] - calage["lat_min"]) / calage["hauteur_m"]

    lon_lo = calage["lon_min"] + col0 * tuile_m * deg_par_m_lon
    lon_hi = lon_lo + n_col * tuile_m * deg_par_m_lon
    # Rangée 0 au NORD : la latitude décroît quand la rangée augmente.
    lat_hi = calage["lat_max"] - rangee0 * tuile_m * deg_par_m_lat
    lat_lo = lat_hi - n_rangee * tuile_m * deg_par_m_lat
    return lon_lo, lon_hi, lat_lo, lat_hi


def lire_points_poser(dossier_carte, calage):
    """Points de la carte où l'on se pose : ses hélisurfaces (helipads.txt) et son
       point de départ (start_x / start_z de terrain.txt, en coordonnées monde,
       ramené en degrés). Ce sont les endroits où l'on descend assez bas pour que
       la finesse du niveau large ne suffise plus."""
    points = []
    chemin = os.path.join(dossier_carte, "helipads.txt")
    if os.path.exists(chemin):
        with open(chemin, encoding="utf-8") as f:
            for ligne in f:
                ligne = ligne.strip()
                if not ligne or ligne.startswith("#"):
                    continue
                morceaux = ligne.split(None, 2)
                if len(morceaux) >= 2:
                    points.append((float(morceaux[0]), float(morceaux[1])))

    depart = calage.get("depart")
    if depart is not None:
        points.append(depart)
    return points


def bloc_dans_zones(lon_lo, lon_hi, lat_lo, lat_hi, zones):
    """Vrai si l'emprise du bloc croise l'une des zones demandées. Le test est
       fait sur les rectangles englobants : un bloc fait près d'un kilomètre de
       côté, affiner le contour ne changerait pratiquement rien au volume
       téléchargé."""
    if not zones:
        return True  # aucune zone demandée : toute la carte
    for z_lon_lo, z_lat_lo, z_lon_hi, z_lat_hi in zones:
        if lon_hi >= z_lon_lo and lon_lo <= z_lon_hi and lat_hi >= z_lat_lo and lat_lo <= z_lat_hi:
            return True
    return False


def zone_autour(lon, lat, rayon_km):
    """Rectangle englobant d'un disque de rayon donné, en degrés. Un degré de
       latitude fait 111 km ; en longitude il rétrécit avec le cosinus de la
       latitude."""
    d_lat = rayon_km / 111.0
    d_lon = rayon_km / (111.0 * max(0.2, math.cos(math.radians(lat))))
    return (lon - d_lon, lat - d_lat, lon + d_lon, lat + d_lat)


def bloc_complet(sortie, col0, rangee0):
    """Vrai si ce bloc a déjà été traité (reprise après interruption). On se fie à
       un fichier de marque plutôt qu'à la présence des tuiles : un bloc peut
       légitimement n'en avoir écrit aucune, si tout y était hors couverture BD
       ORTHO, et on ne veut pas le retélécharger indéfiniment."""
    return os.path.exists(os.path.join(sortie, ".blocs", f"{col0}_{rangee0}"))


def marquer_bloc(sortie, col0, rangee0):
    dossier = os.path.join(sortie, ".blocs")
    os.makedirs(dossier, exist_ok=True)
    with open(os.path.join(dossier, f"{col0}_{rangee0}"), "w", encoding="utf-8") as f:
        f.write("fait\n")


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("carte", help="nom du dossier sous assets/terrain")
    p.add_argument("sortie", help="dossier où écrire les tuiles")
    p.add_argument("--m-par-pixel", type=float, default=0.75)
    p.add_argument("--tuile-px", type=int, default=512)
    p.add_argument("--bloc-tuiles", type=int, default=9)
    p.add_argument("--autour", nargs=3, type=float, action="append", default=[],
                   metavar=("LON", "LAT", "RAYON_KM"))
    p.add_argument("--autour-helipads", type=float, default=0.0, metavar="RAYON_KM")
    p.add_argument("--reprendre", action="store_true")
    p.add_argument("--outil", default=os.path.join("build", "bin", "orthotuiles"))
    args = p.parse_args()

    dossier_carte = os.path.join(config.TERRAIN_ROOT, args.carte)
    terrain_txt = os.path.join(dossier_carte, "terrain.txt")
    if not os.path.exists(terrain_txt):
        raise SystemExit(f"carte inconnue : {terrain_txt}")
    if not os.path.exists(args.outil):
        raise SystemExit(f"outil absent : {args.outil}\n"
                         "Compiler d'abord : cmake --build build --target orthotuiles")

    bloc_px = args.bloc_tuiles * args.tuile_px
    if bloc_px > config.WMS_MAX_PX:
        raise SystemExit(f"bloc de {bloc_px} px : au-dessus de la limite serveur IGN "
                         f"({config.WMS_MAX_PX} px). Baisser --bloc-tuiles.")

    calage = lire_calage(terrain_txt)
    colonnes, rangees, tuile_m = grille(calage, args.tuile_px, args.m_par_pixel)

    os.makedirs(args.sortie, exist_ok=True)
    # L'index d'abord : c'est lui qui fixe la grille, et les découpages de blocs
    # s'y réfèrent au lieu de la recalculer chacun de leur côté.
    subprocess.run([args.outil, dossier_carte, args.sortie, "--index-seul",
                    "--m-par-pixel", str(args.m_par_pixel),
                    "--tuile-px", str(args.tuile_px)], check=True)

    zones = [zone_autour(lon, lat, rayon) for lon, lat, rayon in args.autour]
    if args.autour_helipads > 0.0:
        for lon, lat in lire_points_poser(dossier_carte, calage):
            zones.append(zone_autour(lon, lat, args.autour_helipads))

    total_tuiles = colonnes * rangees
    blocs_x = math.ceil(colonnes / args.bloc_tuiles)
    blocs_y = math.ceil(rangees / args.bloc_tuiles)
    print(f"[tuiles] {args.carte} : {colonnes} x {rangees} = {total_tuiles} tuiles "
          f"de {args.tuile_px} px à {args.m_par_pixel} m/px ({tuile_m:.0f} m au sol)")
    print(f"[tuiles] {blocs_x * blocs_y} blocs de {args.bloc_tuiles} x {args.bloc_tuiles} "
          f"tuiles ({bloc_px} px par requête WMS)")
    if zones:
        print(f"[tuiles] {len(zones)} zone(s) demandée(s) : les blocs qui n'en croisent "
              f"aucune sont laissés vides (le moteur y garde le niveau précédent)")
    sys.stdout.flush()

    faits = 0
    hors_zone = 0
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
            largeur = n_col * args.tuile_px
            hauteur = n_rangee * args.tuile_px
            print(f"[{faits}] bloc ({col0}, {rangee0}) {largeur}x{hauteur} px, WMS...")
            sys.stdout.flush()

            image = request_map(lat_lo, lon_lo, lat_hi, lon_hi, largeur, hauteur)
            # Le bloc n'est qu'un intermédiaire jeté aussitôt découpé. On le
            # repasse en JPEG de très haute qualité plutôt qu'en PNG : à
            # 21 mégapixels par bloc, l'encodage sans perte coûterait plusieurs
            # secondes par bloc pour un gain que la compression BC7 qui suit
            # effacerait de toute façon.
            with tempfile.NamedTemporaryFile(suffix=".jpg", delete=False) as tmp:
                chemin_tmp = tmp.name
            try:
                image.save(chemin_tmp, quality=97)
                cmd = [args.outil, dossier_carte, args.sortie,
                       "--source", chemin_tmp,
                       "--tuile-px", str(args.tuile_px),
                       "--bloc", str(col0), str(rangee0)]
                if args.reprendre:
                    cmd.append("--reprendre")
                subprocess.run(cmd, check=True)
                marquer_bloc(args.sortie, col0, rangee0)
            finally:
                os.unlink(chemin_tmp)

    if hors_zone > 0:
        print(f"[tuiles] {hors_zone} blocs hors des zones demandées, non téléchargés")
    print(f"[tuiles] terminé : {faits} blocs traités, {args.sortie}")


if __name__ == "__main__":
    main()
