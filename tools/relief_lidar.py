#!/usr/bin/env python3
"""
relief_lidar.py
Regénère le relief d'une carte à partir du LiDAR HD de l'IGN, au lieu du RGE
ALTI que fetch_terrain.py utilise. Même service, même format : seule la couche
change (voir tools/lidar/services.py).

Pourquoi. Le RGE ALTI servi à 1 m est un rééchantillonnage d'une pyramide plus
grossière ; à l'ombrage il est lisse et strié. Le MNT LiDAR porte du vrai détail
métrique. Sur une petite carte, où l'on peut se payer une grille fine, l'écart
est visible à l'oeil.

Par défaut l'outil écrit dans une carte NOUVELLE, copie de la source : la carte
d'origine n'est jamais touchée, et les deux se comparent en vol en changeant de
carte dans le menu.

Le LiDAR HD s'arrête à la frontière : 9 % de l'emprise d'ossau et 12 % de celle
de cauterets n'en ont aucune. Ces trous gardent le relief déjà en place, sans
retouche : ils ne peuvent donc pas être dégradés (voir combler).

Attention : le MNT est le sol NU. Les bâtiments n'y sont pas, et c'est ce qu'on
veut pour un relief (le moteur les extrude par ailleurs depuis la BD TOPO). Le
MNS, lui, les inclut : à réserver aux modèles d'objets, pas au terrain sur
lequel on se pose.

Données : IGN Géoplateforme (LiDAR HD), Licence Ouverte Etalab 2.0.
Dépendances : Python 3, numpy, Pillow.

Usage :
  python3 tools/relief_lidar.py dax-arene                    -> dax-arene-lidar
  python3 tools/relief_lidar.py dax-arene --grille 1024      grille plus fine
  python3 tools/relief_lidar.py dax-arene --sortie essai     autre nom de sortie
  python3 tools/relief_lidar.py dax-arene --sur-place        écrase la carte source

Auteur : O. Booklage
Licence : GPL v2
"""

import argparse
import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from terrain import config
from terrain.meta import read_meta, update_keys
from lidar.services import COUCHE_MNS, COUCHE_MNT, grille_altitudes

SURECHANTILLONNAGE = 4


def copier_carte(source, cible):
    """Copie une carte dans un nouveau dossier. Tous les fichiers annexes
       (orthophoto, lieux, hélipads, bâtiments, exclusions) restent valides :
       ils sont en coordonnées géographiques ou monde, que le relief ne change
       pas."""
    cible.mkdir(parents=True, exist_ok=True)
    for fichier in sorted(source.iterdir()):
        if fichier.is_file():
            shutil.copy2(fichier, cible / fichier.name)


# Un point du laser qui plonge de plus de 300 m sous le relief déjà en place ne
# mesure pas le sol : sur ossau, 33 points de ce genre creusaient des puits de
# deux kilomètres. Une gorge réelle ne s'écarte pas d'autant d'un relief qui en
# est le lissage.
CHUTE_ABERRANTE_M = 300.0


# Largeur du raccord entre le laser et le relief en place, en mètres. Un
# remplacement sec laissait des marches de 200 à 340 m entre points voisins le
# long de la frontière espagnole (mesuré le 16/08 sur ossau, cauterets et pau) :
# le laser se fond donc vers la carte à l'approche des zones remplacées.
RACCORD_M = 500.0


def _distance_mailles(masque, portee):
    """Distance de chaque point au masque, en mailles, plafonnée à portee.
       Par dilatations successives SANS repliement (jamais np.roll : un bord de
       carte irait chercher le bord opposé, l'erreur a déjà été payée)."""
    d = np.full(masque.shape, float(portee))
    d[masque] = 0.0
    front = masque.copy()
    for k in range(1, portee):
        voisin = np.zeros_like(front)
        voisin[1:, :] |= front[:-1, :]
        voisin[:-1, :] |= front[1:, :]
        voisin[:, 1:] |= front[:, :-1]
        voisin[:, :-1] |= front[:, 1:]
        nouveau = voisin & (d >= portee)
        if not nouveau.any():
            break
        d[nouveau] = float(k)
        front |= voisin
    return d


def combler(altitudes, carte, maille_m):
    """Remplace par le relief déjà en place ce que le laser ne donne pas, ou
       donne de travers, avec un RACCORD en fondu sur RACCORD_M mètres. Renvoie
       les altitudes et les deux pourcentages (absent, aberrant).

       Le LiDAR HD s'arrête à la frontière : sur ossau et cauterets, le versant
       espagnol n'a aucune donnée, et le laisser à zéro y creuserait la mer.

       Dans les zones remplacées, le relief en place ressort tel quel. Autour,
       le laser glisse vers la carte sur la largeur du raccord : sans ce fondu,
       l'écart entre les deux relevés (écart-type 20 m, queues à 300) faisait de
       vraies falaises le long de la ligne de couverture. Le prix : le versant
       français est adouci sur ~500 m le long de cette ligne."""
    absent = altitudes <= config.NODATA
    # Un point absent vaut -1001 : il plonge donc sous la carte lui aussi, et
    # serait compté deux fois. On ne juge que les points effectivement mesurés.
    aberrant = ~absent & (altitudes < carte - CHUTE_ABERRANTE_M)
    remplace = absent | aberrant
    if not remplace.any():
        return altitudes, 0.0, 0.0, remplace

    portee = max(4, int(round(RACCORD_M / maille_m)))
    distance = _distance_mailles(remplace, portee)
    poids = np.clip(distance / float(portee), 0.0, 1.0)
    laser = np.where(remplace, carte, altitudes)
    return (carte + poids * (laser - carte),
            float(np.mean(absent)) * 100.0,
            float(np.mean(aberrant)) * 100.0,
            distance < portee)


def marches_par_zone(altitudes, zone):
    """Plus grand dénivelé entre voisins, séparé entre les paires touchant la
       zone de raccord et les autres. C'est la séparation qui juge : le laser
       porte de VRAIES parois (183,8 m mesurés sur cauterets, que le RGE lissait
       à 59), un seuil absolu les confondrait avec une couture fabriquée. Le bon
       critère est que la couture ne dépasse pas nettement le relief
       authentique de la carte."""
    dans = 0.0
    hors = 0.0
    for axe in (0, 1):
        dif = np.abs(np.diff(altitudes, axis=axe))
        touche = zone[:-1, :] | zone[1:, :] if axe == 0 else zone[:, :-1] | zone[:, 1:]
        if touche.any():
            dans = max(dans, float(dif[touche].max()))
        if (~touche).any():
            hors = max(hors, float(dif[~touche].max()))
    return dans, hors


def lire_heightmap(dossier, meta):
    """Relit le relief déjà en place d'une carte, en mètres."""
    niveaux = np.asarray(Image.open(dossier / "heightmap.png")).astype(np.float64)
    return niveaux / 65535.0 * float(meta["elev_max"])


def ecrire_heightmap(chemin, altitudes):
    """Écrit la grille en PNG 16 bits, normalisée entre 0 et son maximum, comme
       le fait terrain/relief.py. Renvoie l'altitude maximale."""
    propre = np.where(altitudes > config.NODATA, np.maximum(altitudes, 0.0), 0.0)
    maximum = float(propre.max())
    etendue = maximum if maximum > 0.0 else 1.0
    niveaux = np.round(propre / etendue * 65535.0).astype(np.uint16)
    Image.fromarray(niveaux, mode="I;16").save(chemin)
    return maximum


def main():
    analyseur = argparse.ArgumentParser(description=__doc__.split("\n")[2])
    analyseur.add_argument("carte", help="carte source (dossier sous assets/terrain/)")
    analyseur.add_argument("--sortie", default=None,
                           help="nom de la carte à écrire (défaut : <carte>-lidar)")
    analyseur.add_argument("--sur-place", action="store_true",
                           help="écrase le relief de la carte source, sans copie")
    analyseur.add_argument("--grille", type=int, default=None,
                           help="nombre de points par côté (défaut : celui de la carte)")
    analyseur.add_argument("--surech", type=int, default=SURECHANTILLONNAGE,
                           help="suréchantillonnage avant moyenne (défaut 4)")
    analyseur.add_argument("--surface", action="store_true",
                           help="prend le MNS (bâti et végétation compris) au lieu du sol nu")
    args = analyseur.parse_args()

    source = Path(config.TERRAIN_ROOT) / args.carte
    if not (source / "terrain.txt").exists():
        raise RuntimeError(f"carte inconnue : {source}")
    cible = source if args.sur_place else \
        Path(config.TERRAIN_ROOT) / (args.sortie or f"{args.carte}-lidar")

    meta = read_meta(source / "terrain.txt")
    bornes = (float(meta["lon_min"]), float(meta["lon_max"]),
              float(meta["lat_min"]), float(meta["lat_max"]))
    nx = nz = args.grille or int(meta["cols"])
    if args.grille is None:
        nx, nz = int(meta["cols"]), int(meta["rows"])

    largeur_m, hauteur_m = float(meta["width_m"]), float(meta["height_m"])
    print(f"[carte] {args.carte} -> {cible.name} : {largeur_m:.0f} x {hauteur_m:.0f} m, "
          f"grille {nx}x{nz}, maille {largeur_m / (nx - 1):.1f} m "
          f"(avant : {largeur_m / (int(meta['cols']) - 1):.1f} m)")

    if cible != source:
        copier_carte(source, cible)

    couche = COUCHE_MNS if args.surface else COUCHE_MNT
    altitudes = grille_altitudes(couche, bornes, nx, nz, max(1, args.surech))

    # Le LiDAR HD s'arrête à la frontière : sans ce rattrapage, le versant
    # espagnol d'ossau ou de cauterets tomberait au niveau de la mer.
    zone = None
    if nx == int(meta["cols"]) and nz == int(meta["rows"]):
        altitudes, absent, aberrant, zone = combler(altitudes, lire_heightmap(source, meta),
                                                    largeur_m / (nx - 1))
        if absent > 0.0:
            print(f"[relief] {absent:.1f} % de l'emprise sans donnée LiDAR, "
                  f"relief en place conservé")
        if aberrant > 0.0:
            print(f"[relief] {aberrant:.3f} % de points aberrants écartés "
                  f"(plus de {CHUTE_ABERRANTE_M:.0f} m sous le relief en place)")
    elif float(np.mean(altitudes <= config.NODATA)) > 0.0:
        print("[relief] grille redimensionnée : les trous ne peuvent pas être comblés "
              "depuis le relief en place, ils resteront au niveau 0")

    # Auto-contrôle (leçon du 16/08 : trois cartes livrées avec des marches de
    # 300 m à la frontière espagnole) : la couture du raccord ne doit pas
    # dépasser nettement les parois RÉELLES de la carte. On refuse de livrer
    # sinon, plutôt que de compter sur une relecture humaine.
    if zone is not None and zone.any():
        dans, hors = marches_par_zone(altitudes, zone)
        print(f"[relief] marche maximale : {dans:.1f} m au raccord, {hors:.1f} m ailleurs")
        if dans > max(150.0, 1.25 * hors):
            raise RuntimeError(f"raccord à {dans:.1f} m contre {hors:.1f} m de relief "
                               f"réel : heightmap refusée, rien n'est écrit")

    maximum = ecrire_heightmap(cible / "heightmap.png", altitudes)
    update_keys(cible / "terrain.txt",
                {"cols": nx, "rows": nz, "elev_max": f"{maximum:.2f}"})
    print(f"[relief] {cible / 'heightmap.png'} écrit, altitude 0 -> {maximum:.1f} m")
    print(f"[ok] carte {cible.name} prête ; la choisir dans le menu des cartes "
          f"ou par ARTOUSTE_TERRAIN={cible.name}")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
