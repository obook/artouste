#!/usr/bin/env python3
"""
Nom du fichier : tuiles_grille.py
Description : Calage d'une carte, grille de tuiles, blocs et marqueurs de
              fabrication inachevée.
Auteur : O. Booklage
Date : Août 2026
Licence : GPL v2
"""

import math
import os

NOM_MARQUEUR_INACHEVE = "fabrication_inachevee.txt"

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


def marquer_inacheve(sortie, m_par_pixel, tuiles_attendues):
    """Pose le témoin d'inachèvement, en même temps que l'index et dans les mêmes
       termes que la fabrication intégrée au jeu (voir
       src/app/cartes/FabriqueTuiles.hpp). L'index décrit la grille VOULUE : sans
       ce témoin, un jeu interrompu par ce script ne se distingue pas d'un jeu
       complet, et l'écran des cartes annonce des tuiles qui ne couvrent qu'un
       coin de la carte."""
    with open(os.path.join(sortie, NOM_MARQUEUR_INACHEVE), "w", encoding="utf-8") as f:
        f.write("# Fabrication en cours ou interrompue.\n")
        f.write("# Ce fichier disparaît quand le jeu de tuiles est complet.\n")
        f.write("# Relancer la fabrication reprend où elle s'est arrêtée.\n")
        f.write(f"m_par_pixel {m_par_pixel}\n")
        f.write(f"tuiles_attendues {tuiles_attendues}\n")


def retirer_marque_inacheve(sortie):
    """Retire le témoin. Seul appelant : la fin normale de la boucle, une fois
       tous les blocs demandés traités. Une coupure sort avant et le laisse en
       place, ce qui est tout l'intérêt."""
    try:
        os.unlink(os.path.join(sortie, NOM_MARQUEUR_INACHEVE))
    except FileNotFoundError:
        pass


def marquer_bloc(sortie, col0, rangee0):
    dossier = os.path.join(sortie, ".blocs")
    os.makedirs(dossier, exist_ok=True)
    with open(os.path.join(dossier, f"{col0}_{rangee0}"), "w", encoding="utf-8") as f:
        f.write("fait\n")
