#!/usr/bin/env python3
"""
fetch_forest.py
Fabrique le masque de forêt d'une carte (forest.png), que le moteur consulte pour
semer les arbres. Trois couches de l'IGN (Géoplateforme, Licence Ouverte Etalab
2.0), chacune pour ce qu'elle sait faire :

  1. contours de départements (ADMIN EXPRESS) : où s'arrête le territoire
     couvert. Hors de France, le moteur retombe sur la couleur de l'orthophoto ;
     en France, l'absence de végétation cartographiée VAUT absence d'arbres.
  2. zone_de_vegetation (BD TOPO) : le CONTOUR. Levé topographique, sans seuil de
     surface : il découpe les stades, les autoroutes, les aires de repos et les
     parkings, et connaît les haies et les bosquets de ville.
  3. formation_vegetale (BD Forêt V2) : l'ESSENCE dominante, que la BD TOPO ne
     donne pas (elle dit "Bois", pas "pin maritime"). Elle généralise à 0,5 ha,
     ses contours débordent donc sur les stades et les routes : on ne s'en sert
     QUE pour colorer l'intérieur du contour BD TOPO.

Le semis ne jugeait auparavant "c'est de la forêt" que sur la couleur du pixel
d'orthophoto (VegetationScatter.cpp, looksLikeForest) : il plantait sur une
pelouse sombre ou une ombre de versant, et oubliait une forêt en plein soleil.

L'emprise est lue dans le terrain.txt de la carte, pas dans zones/ : une carte
recadrée (crop_zombie_map.py) a son propre terrain.txt et fonctionne donc aussi.
Le masque couvre EXACTEMENT la même emprise que l'orthophoto, ce qui évite tout
recalage côté moteur (mêmes proportions, mêmes bornes).

Format de forest.png : image en niveaux de gris, un pixel tous les PX_M mètres.
    0    hors territoire français : le moteur retombe sur le test de couleur
    40   en France et non boisé : aucun arbre
    85   feuillus
    130  pins (pin maritime des Landes, pin sylvestre, pin à crochets...)
    170  autres conifères (sapin, épicéa, douglas, mélèze)
    255  mélange feuillus / conifères
Ces niveaux sont espacés pour rester lisibles à l'oeil dans une visionneuse : le
masque se contrôle en l'ouvrant, superposé mentalement à ortho.jpg.

Données : IGN Géoplateforme (ADMIN EXPRESS, BD TOPO, BD Forêt V2), Licence
Ouverte Etalab 2.0.
Dépendances : Python 3 + Pillow + NumPy (déjà requis par le téléchargeur de
terrain).

Usage : python3 tools/fetch_forest.py [carte]   (carte par défaut : ossau)
        python3 tools/fetch_forest.py --test    (contrôle de bon sens)
Sortie : assets/terrain/<carte>/forest.png

Auteur : O. Booklage
Licence : GPL v2
"""

import os
import sys
import time
import urllib.error
from pathlib import Path

import numpy as np
from PIL import Image

# On réutilise la lecture de terrain.txt du paquet terrain : une seule source de
# vérité pour l'emprise géographique de la carte.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from terrain.config import TERRAIN_ROOT
from terrain.meta import read_meta

from foret.essences import essence_bdforet, essence_bdtopo, porte_des_arbres
from foret.geometrie import en_pixels, epaisseur_route, surface
from foret.masque import gommer, peindre
from foret.reglages import (CHAUSSEE_MIN_M, CONIFERE, FEUILLU, MIXTE,
                            NATURES_ROUTE, NON_BOISE, PIN, PX_M, WFS_LAYER_DEPT,
                            WFS_LAYER_ESSENCE, WFS_LAYER_VEGETATION)
from foret.service_ign import fetch_pages, filtre_natures


def autotest():
    """Contrôle de bon sens (python3 tools/fetch_forest.py --test) : classement
       des essences, tri des natures BD TOPO et sens des axes du masque, les
       endroits où une erreur ne se verrait qu'en vol."""
    assert essence_bdforet("Lande", "NC") == 0
    assert essence_bdforet("Formation herbacée", "NC") == 0
    assert essence_bdforet("Forêt fermée de hêtre pur", "Hêtre") == FEUILLU
    assert essence_bdforet("Forêt fermée de sapin ou épicéa", "Sapin, épicéa") == CONIFERE
    assert essence_bdforet("Forêt ouverte de conifères purs", "Conifères") == CONIFERE
    assert essence_bdforet("Forêt fermée de pin maritime pur", "Pin maritime") == PIN
    assert essence_bdforet("Forêt fermée de pin à crochets ou pin cembro pur",
                           "Pin à crochets, pin cembro") == PIN
    assert essence_bdforet("Forêt fermée à mélange de feuillus prépondérants et conifères",
                           "Mixte") == MIXTE

    assert porte_des_arbres("Bois")
    assert porte_des_arbres("Forêt fermée de conifères")
    assert porte_des_arbres("Haie")
    assert not porte_des_arbres("Lande ligneuse")
    assert not porte_des_arbres("Vigne")
    assert essence_bdtopo("Forêt fermée de conifères") == CONIFERE
    assert essence_bdtopo("Forêt fermée mixte") == MIXTE
    assert essence_bdtopo("Bois") == FEUILLU

    # Épaisseur de gomme : une autoroute (chaussée 10 m) doit couvrir plus d'un
    # pixel, une bretelle sans largeur renseignée doit quand même être gommée.
    assert epaisseur_route(10) == 2, epaisseur_route(10)
    assert epaisseur_route(None) >= 1
    assert epaisseur_route(0) == epaisseur_route(CHAUSSEE_MIN_M)

    # Le filtre OGC porte l'emprise en latitude longitude, dans cet ordre.
    f = filtre_natures("-1.5,43.5,-1.3,43.8", ("Type autoroutier",))
    assert "<gml:lowerCorner>43.5 -1.5</gml:lowerCorner>" in f, f
    assert "<fes:Literal>Type autoroutier</fes:Literal>" in f
    assert "Chemin" not in " ".join(NATURES_ROUTE)  # chemins et sentiers épargnés

    cadre = (-1.0, 1.0, 42.0, 43.0, 101, 51)  # 101x51 px sur 2 deg x 1 deg
    coins = en_pixels([(-1.0, 43.0), (1.0, 42.0)], cadre)
    assert coins[0] == (0.0, 0.0), coins            # nord-ouest = coin haut gauche
    assert coins[1] == (100.0, 50.0), coins         # sud-est = coin bas droite
    assert abs(surface([(0, 0), (10, 0), (10, 4), (0, 4)]) - 40.0) < 1e-9
    print("[forêt] autotest : ok")


def main():
    carte = sys.argv[1] if len(sys.argv) > 1 else "ossau"
    if carte == "--test":
        autotest()
        return
    out_dir = Path(TERRAIN_ROOT) / carte
    meta_path = out_dir / "terrain.txt"
    if not meta_path.exists():
        raise RuntimeError(f"carte inconnue : {carte} (pas de {meta_path})")
    meta = read_meta(meta_path)

    lon_min, lon_max = float(meta["lon_min"]), float(meta["lon_max"])
    lat_min, lat_max = float(meta["lat_min"]), float(meta["lat_max"])
    width_m, height_m = float(meta["width_m"]), float(meta["height_m"])
    bbox = f"{lon_min},{lat_min},{lon_max},{lat_max}"

    # Grille du masque : mêmes bornes que l'orthophoto, résolution PX_M.
    cols = max(1, round(width_m / PX_M))
    rows = max(1, round(height_m / PX_M))
    cadre = (lon_min, lon_max, lat_min, lat_max, cols, rows)

    print(f"[forêt] carte {carte} : emprise {bbox}, masque {cols}x{rows} "
          f"({PX_M:.0f} m/px)")
    depart = time.time()

    france, n_dept = peindre(cadre, fetch_pages(bbox, WFS_LAYER_DEPT),
                             lambda p: NON_BOISE)
    print(f"[forêt]   {n_dept} contours de départements")

    arbres, n_veg = peindre(cadre, fetch_pages(bbox, WFS_LAYER_VEGETATION),
                            lambda p: essence_bdtopo(p.get("nature"))
                            if porte_des_arbres(p.get("nature")) else 0)
    print(f"[forêt]   {n_veg} zones de végétation (BD TOPO)")

    essences, n_ess = peindre(cadre, fetch_pages(bbox, WFS_LAYER_ESSENCE, "tfv,essence,geom"),
                              lambda p: essence_bdforet(p.get("tfv") or "",
                                                        p.get("essence") or ""))
    print(f"[forêt]   {n_ess} formations de la BD Forêt (essences)")

    # Assemblage : le contour vient de la BD TOPO, l'essence de la BD Forêt quand
    # elle en donne une à cet endroit, sinon celle déduite de la nature BD TOPO.
    masque = np.where(arbres > 0, np.where(essences > 0, essences, arbres), france)

    # Passe de gomme, en dernier. Bornée au territoire français (masque > 0) pour
    # ne pas transformer l'étranger, où l'on n'a pas de donnée, en "non boisé".
    gomme, n_routes, n_surfaces = gommer(cadre, bbox)
    print(f"[forêt]   gomme : {n_routes} tronçons de chaussée, "
          f"{n_surfaces} terrains de sport et pistes")
    masque = np.where((gomme > 0) & (masque > 0), NON_BOISE, masque)

    Image.fromarray(masque.astype(np.uint8)).save(out_dir / "forest.png", optimize=True)

    path = out_dir / "forest.png"
    total = float(cols * rows)
    boise = (masque > NON_BOISE).sum() / total
    en_france = (masque > 0).sum() / total
    taille_ko = os.path.getsize(path) / 1024.0
    print(f"[forêt] {path} écrit : {boise * 100.0:.1f} % de la carte boisée, "
          f"{en_france * 100.0:.1f} % en France, {taille_ko:.0f} ko")
    print(f"[ok] terminé en {time.time() - depart:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
