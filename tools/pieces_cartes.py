#!/usr/bin/env python3
"""
pieces_cartes.py
Fabrique les pièces de surface de TOUTES les cartes, une par hélisurface qui le
mérite, et écrit les lignes de déclaration correspondantes.

Une pièce de surface (voir tools/piece_surface.py) remplace localement le relief
de la carte et ses bâtiments extrudés par la surface réelle relevée au laser.
Elle n'a d'intérêt que là où l'on descend bas et où il y a quelque chose à voir :
les hélisurfaces sont donc les bons candidats, mais toutes ne le méritent pas.
Une hélisurface au milieu d'un pré ne gagnerait qu'un pré plus cher.

Le tri est mesuré, pas deviné : on sonde le relevé laser autour de chaque
hélisurface et on garde celles dont la part bâtie dépasse un seuil, puis les
mieux pourvues dans la limite d'un budget par carte.

Deux budgets à ne pas perdre de vue, et c'est pourquoi cet outil existe plutôt
qu'une boucle à la main :

  - le dessin : les monuments sont dessinés à chaque image, sans tri par
    distance. Trois pièces par carte coûtent environ 0,4 ms, trente en
    coûteraient dix fois plus ;
  - le dépôt : une pièce pèse quelques mégaoctets, et assets/ en fait déjà 335.

Usage :
  python3 tools/pieces_cartes.py --simuler          sonde et chiffre, sans rien écrire
  python3 tools/pieces_cartes.py                    fabrique
  python3 tools/pieces_cartes.py --cartes dax pau   se limite à ces cartes

Auteur : O. Booklage
Licence : GPL v2
"""

import argparse
import math
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from terrain import config
from terrain.meta import read_meta
from lidar.carte import MARGE_FONDU
from lidar.piece import fabriquer, verifier
from lidar.services import COUCHE_MNS, COUCHE_MNT, ORTHO_M_PAR_PX, grille_altitudes

RACINE = Path(__file__).resolve().parent.parent

# Emprise et finesse d'une pièce de série. Plus petite et plus lâche que celles
# du Pic du Midi et de Dax, faites à la main : à trente exemplaires, la question
# n'est plus la beauté d'une pièce mais le poids de toutes.
DEMI_X, DEMI_Z = 200.0, 150.0
PAS = 1.5
PART_BATIE_MIN = 0.03   # sous 3 % de bâti dans l'emprise, la pièce n'apporte rien
# Hauteur bâtie sous l'hélisurface elle-même au-delà de laquelle on renonce.
# Une pièce ne change pas l'altitude du terrain : elle DESSINE le bâtiment, mais
# l'appareil se pose toujours au niveau du sol, donc à travers lui. Mesuré : dix
# hélisurfaces du dépôt sont sur des toits, jusqu'à +33 m à la Pitié-Salpêtrière.
# Les traiter demanderait que le moteur sache poser sur la surface d'une pièce.
HAUTEUR_PAD_MAX = 2.0
PIECES_PAR_CARTE = 3

# Cartes d'essai et de jeu à ne pas traiter.
CARTES_ECARTEES = {"dax-arene", "dax-arene-lidar", "dax-arene-mns", "bigorre-lidar"}


def cartes_du_depot():
    racine = Path(config.TERRAIN_ROOT)
    return sorted(d.name for d in racine.iterdir()
                  if (d / "terrain.txt").exists() and d.name not in CARTES_ECARTEES)


def helipads(carte):
    """Hélisurfaces d'une carte : (nom, lon, lat)."""
    chemin = Path(config.TERRAIN_ROOT) / carte / "helipads.txt"
    if not chemin.exists():
        return []
    lieux = []
    for ligne in chemin.read_text(encoding="utf-8").splitlines():
        ligne = ligne.strip()
        if not ligne or ligne.startswith("#"):
            continue
        champs = ligne.split(None, 2)
        if len(champs) >= 3:
            lieux.append((champs[2].strip(), float(champs[0]), float(champs[1])))
    return lieux


def hauteur_sous_pad(lon, lat):
    """Hauteur du bâti sous l'hélisurface elle-même, sur une fenêtre de 20 m."""
    dlat = 10.0 / 111320.0
    dlon = 10.0 / (111320.0 * math.cos(math.radians(lat)))
    bornes = (lon - dlon, lon + dlon, lat - dlat, lat + dlat)
    mnt = grille_altitudes(COUCHE_MNT, bornes, 10, 10, surech=1, journal=False)
    mns = grille_altitudes(COUCHE_MNS, bornes, 10, 10, surech=1, journal=False)
    if (mnt <= config.NODATA).any() or (mns <= config.NODATA).any():
        return -1.0
    return float(np.median(mns - mnt))


def part_batie(lon, lat, demi_x, demi_z, pas=4.0):
    """Part de l'emprise couverte par du bâti, d'après le relevé laser. Sonde
       grossière (4 m) : on ne cherche pas la forme, seulement la présence."""
    dlat = 1.0 / 111320.0
    dlon = 1.0 / (111320.0 * math.cos(math.radians(lat)))
    bornes = (lon - demi_x * dlon, lon + demi_x * dlon,
              lat - demi_z * dlat, lat + demi_z * dlat)
    nx = max(8, int(round(2 * demi_x / pas)))
    nz = max(8, int(round(2 * demi_z / pas)))
    mnt = grille_altitudes(COUCHE_MNT, bornes, nx, nz, surech=1, journal=False)
    mns = grille_altitudes(COUCHE_MNS, bornes, nx, nz, surech=1, journal=False)
    if (mnt <= config.NODATA).any() or (mns <= config.NODATA).any():
        return -1.0    # hors couverture LiDAR
    return float(np.mean((mns - mnt) > 2.0))


def reglages(carte, nom, lon, lat, args):
    """Le jeu d'arguments qu'attend lidar.piece.fabriquer, pour une pièce de série."""
    court = "".join(c.lower() if c.isalnum() else "-" for c in nom).strip("-")
    court = "-".join(m for m in court.split("-") if m)[:40]
    reglage = argparse.Namespace(
        carte=carte, nom=f"{carte}-{court}", titre=nom, lon=lon, lat=lat,
        demi_x=args.demi_x, demi_z=args.demi_z, pas=args.pas,
        marge=MARGE_FONDU, ortho_m_par_px=ORTHO_M_PAR_PX,
        coupoles=False, facade=None)
    reglage.sortie = RACINE / "assets" / "models" / "monuments" / reglage.nom
    return reglage


def declarer(reglage, altitude):
    """Ajoute la pièce au monuments.txt de la carte et son cercle au
       exclusions.txt, sans doublon : l'outil est fait pour être relancé."""
    dossier = Path(config.TERRAIN_ROOT) / reglage.carte
    rayon = math.hypot(reglage.demi_x, reglage.demi_z)

    monuments = dossier / "monuments.txt"
    if not monuments.exists():
        monuments.write_text(
            "# Monuments 3D posés sur la carte (un par ligne). Format complet et\n"
            "# explications dans assets/terrain/paris/monuments.txt.\n"
            "#\n"
            "# Format : lon lat altitude cap echelle_h echelle_v rayon_m fichier nom\n",
            encoding="utf-8")
    lignes = monuments.read_text(encoding="utf-8")
    marque = f"{reglage.nom}/{reglage.nom}.glb"
    if marque not in lignes:
        with monuments.open("a", encoding="utf-8") as f:
            f.write(f"{reglage.lon:.5f} {reglage.lat:.5f} {altitude:.1f} 0 1 1 "
                    f"{rayon:.0f} {marque} {reglage.titre}\n")

    exclusions = dossier / "exclusions.txt"
    entete = ("# Zones d'exclusion de végétation (un cercle par ligne : lon lat rayon_m).\n"
              "# Aucun arbre n'est planté dans ces cercles.\n")
    texte = exclusions.read_text(encoding="utf-8") if exclusions.exists() else entete
    repere = f"# pièce de surface {reglage.nom}"
    if repere not in texte:
        texte += f"{reglage.lon:.5f} {reglage.lat:.5f} {rayon:.0f}   {repere}\n"
        exclusions.write_text(texte, encoding="utf-8")


def main():
    analyseur = argparse.ArgumentParser(description=__doc__.split("\n")[2])
    analyseur.add_argument("--cartes", nargs="*", default=None)
    analyseur.add_argument("--simuler", action="store_true",
                           help="sonde et chiffre, sans fabriquer ni écrire")
    analyseur.add_argument("--demi-x", type=float, default=DEMI_X)
    analyseur.add_argument("--demi-z", type=float, default=DEMI_Z)
    analyseur.add_argument("--pas", type=float, default=PAS)
    analyseur.add_argument("--par-carte", type=int, default=PIECES_PAR_CARTE)
    analyseur.add_argument("--part-min", type=float, default=PART_BATIE_MIN)
    args = analyseur.parse_args()

    cartes = args.cartes or cartes_du_depot()
    triangles_piece = 2 * round(2 * args.demi_x / args.pas) * round(2 * args.demi_z / args.pas)
    print(f"[série] pièces de {2 * args.demi_x:.0f} x {2 * args.demi_z:.0f} m au pas de "
          f"{args.pas} m, soit {triangles_piece} triangles chacune")
    print(f"[série] au plus {args.par_carte} par carte, part bâtie minimale "
          f"{100 * args.part_min:.0f} %\n")

    retenues = {}
    for carte in cartes:
        lieux = helipads(carte)
        if not lieux:
            print(f"{carte:<14} aucune hélisurface déclarée")
            continue
        mesures, sur_toit = [], []
        for nom, lon, lat in lieux:
            part = part_batie(lon, lat, args.demi_x, args.demi_z)
            if part >= args.part_min and hauteur_sous_pad(lon, lat) > HAUTEUR_PAD_MAX:
                sur_toit.append(nom)
                continue
            mesures.append((part, nom, lon, lat))
        mesures.sort(reverse=True)
        gardees = [m for m in mesures if m[0] >= args.part_min][:args.par_carte]
        retenues[carte] = gardees
        detail = ", ".join(f"{nom} {100 * part:.0f} %" for part, nom, _, _ in mesures
                           if part >= 0)
        hors = sum(1 for m in mesures if m[0] < 0)
        print(f"{carte:<14} {len(gardees)} retenue(s) sur {len(lieux)} : {detail}"
              + (f" ({hors} hors couverture)" if hors else ""))
        if sur_toit:
            print(f"{'':<14} écartées, hélisurface sur un toit : {', '.join(sur_toit)}")

    total = sum(len(v) for v in retenues.values())
    print(f"\n[série] {total} pièces au total, "
          f"{total * triangles_piece / 1e6:.1f} M triangles cumulés, "
          f"environ {total * triangles_piece * 0.19 / 166412:.1f} ms si une carte les portait toutes")
    par_carte = max((len(v) for v in retenues.values()), default=0)
    print(f"[série] au plus {par_carte} sur une même carte, soit environ "
          f"{par_carte * triangles_piece * 0.19 / 166412:.2f} ms par image")

    if args.simuler:
        print("\n[série] simulation seulement, rien n'a été fabriqué")
        return

    debut = time.time()
    for carte, gardees in retenues.items():
        for part, nom, lon, lat in gardees:
            print(f"\n=== {carte} : {nom} ({100 * part:.0f} % bâti) ===")
            reglage = reglages(carte, nom, lon, lat, args)
            altitude = fabriquer(reglage)
            verifier(reglage)
            declarer(reglage, altitude)
    print(f"\n[ok] {total} pièces fabriquées en {time.time() - debut:.0f} s")
    print("[série] reportez les lignes affichées dans les monuments.txt et "
          "exclusions.txt des cartes concernées")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
