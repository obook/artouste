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

import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

# On réutilise la lecture de terrain.txt du paquet terrain : une seule source de
# vérité pour l'emprise géographique de la carte.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from terrain.config import TERRAIN_ROOT
from terrain.meta import read_meta

# --- Services WFS ------------------------------------------------------------
WFS_URL = "https://data.geopf.fr/wfs/ows"
WFS_LAYER_DEPT = "ADMINEXPRESS-COG-CARTO.LATEST:departement"
WFS_LAYER_VEGETATION = "BDTOPO_V3:zone_de_vegetation"
WFS_LAYER_ESSENCE = "LANDCOVER.FORESTINVENTORY.V2:formation_vegetale"
# Couches de la passe de gomme (voir gommer) : chaussées, terrains de sport et
# pistes d'aérodrome, repeints en non boisé une fois le masque assemblé.
WFS_LAYER_ROUTE = "BDTOPO_V3:troncon_de_route"
WFS_LAYER_SPORT = "BDTOPO_V3:terrain_de_sport"
WFS_LAYER_PISTE = "BDTOPO_V3:piste_d_aerodrome"

# Nombre d'entités par requête (le service pagine ; on avance de l'effectif
# réellement renvoyé, comme fetch_buildings.py).
PAGE = 1000

# Résolution du masque, en mètres par pixel. Assez fin pour que le couloir d'une
# autoroute (25 m) reste dégagé, sans faire enfler le fichier.
PX_M = 10.0

# Niveaux de gris du masque (voir le format ci-dessus).
NON_BOISE = 40
FEUILLU = 85
PIN = 130
CONIFERE = 170
MIXTE = 255

# Natures de la BD TOPO qui portent des arbres. Écartées : lande ligneuse, vigne,
# verger, marais, canne à sucre... végétation sans houppier à l'échelle du semis.
NATURES_ARBOREES = ("bois", "forêt", "peupleraie", "haie", "zone arborée")

# Natures de route goudronnées, gommées du masque. Le chemin, le sentier, la
# route empierrée et l'escalier sont laissés : ils passent SOUS le couvert en
# vraie forêt, et les gommer perforerait le semis de tranchées imaginaires.
NATURES_ROUTE = ("Type autoroutier", "Route à 2 chaussées", "Route à 1 chaussée",
                 "Bretelle", "Rond-point")

# Chaussée de repli quand la BD TOPO ne la renseigne pas (fréquent sur les
# bretelles et les ronds-points), et marge dégagée de part et d'autre :
# accotements, bandes d'arrêt, glissières. En mètres.
CHAUSSEE_MIN_M = 4.0
MARGE_ROUTE_M = 6.0

# Mot entier : "sapin" ne doit pas être pris pour un pin.
RE_PIN = re.compile(r"\bpins?\b")
# Autres essences résineuses de la BD Forêt, reconnues par mot-clé.
MOTS_CONIFERE = ("conifère", "sapin", "épicéa", "douglas", "mélèze", "cyprès",
                 "cèdre")


def filtre_natures(bbox, natures):
    """Filtre OGC (paramètre FILTER, FES 2.0) croisant l'emprise et une liste de
       natures. Ce service ne connaît pas CQL_FILTER (erreur 500), et refuse
       FILTER en même temps que le paramètre BBOX : l'emprise doit donc voyager
       DANS le filtre. Attention, gml:Envelope se donne en LATITUDE LONGITUDE."""
    lon_min, lat_min, lon_max, lat_max = bbox.split(",")
    ors = "".join(
        "<fes:PropertyIsEqualTo><fes:ValueReference>nature</fes:ValueReference>"
        f"<fes:Literal>{n}</fes:Literal></fes:PropertyIsEqualTo>" for n in natures)
    return (
        '<fes:Filter xmlns:fes="http://www.opengis.net/fes/2.0" '
        'xmlns:gml="http://www.opengis.net/gml/3.2"><fes:And>'
        '<fes:BBOX><fes:ValueReference>geometrie</fes:ValueReference>'
        '<gml:Envelope srsName="urn:ogc:def:crs:EPSG::4326">'
        f'<gml:lowerCorner>{lat_min} {lon_min}</gml:lowerCorner>'
        f'<gml:upperCorner>{lat_max} {lon_max}</gml:upperCorner>'
        '</gml:Envelope></fes:BBOX>'
        f'<fes:Or>{ors}</fes:Or>'
        '</fes:And></fes:Filter>')


def fetch_pages(bbox, layer, champs=None, filtre=None):
    """Récupère TOUTES les entités d'une couche sur l'emprise, page par page.
       filtre (voir filtre_natures) remplace le BBOX par un filtre OGC complet."""
    entites = []
    start = 0
    while True:
        params = {
            "SERVICE": "WFS",
            "VERSION": "2.0.0",
            "REQUEST": "GetFeature",
            "TYPENAMES": layer,
            "SRSNAME": "EPSG:4326",
            "OUTPUTFORMAT": "application/json",
            "COUNT": str(PAGE),
            "STARTINDEX": str(start),
        }
        if filtre:
            params["FILTER"] = filtre
        else:
            params["BBOX"] = bbox + ",EPSG:4326"
        if champs:
            params["PROPERTYNAME"] = champs
        page = fetch_page(WFS_URL + "?" + urllib.parse.urlencode(params), layer, start)
        if not page:
            return entites
        entites += page
        start += len(page)


def fetch_page(url, layer, start):
    """Une page, avec reprise sur erreur (le service est parfois capricieux)."""
    last_err = None
    for attempt in range(6):
        try:
            with urllib.request.urlopen(url, timeout=300) as resp:
                return json.loads(resp.read())["features"]
        except urllib.error.HTTPError as err:
            last_err = err
            time.sleep((5.0 if err.code == 429 else 2.0) * (attempt + 1))
        except Exception as err:  # réseau capricieux : on retente
            last_err = err
            time.sleep(1.0 + attempt)
    raise RuntimeError(f"page {layer} (start={start}) : échec ({last_err})")


def porte_des_arbres(nature):
    """La nature BD TOPO donnée porte-t-elle des arbres au sens du semis ?"""
    bas = (nature or "").lower()
    return any(mot in bas for mot in NATURES_ARBOREES)


def essence_bdtopo(nature):
    """Essence de repli déduite de la nature BD TOPO, là où la BD Forêt ne dit
       rien (haie, bosquet, bois sans inventaire). "Bois" ne précisant pas
       l'essence, on le traite en feuillu."""
    bas = (nature or "").lower()
    if "mixte" in bas:
        return MIXTE
    return CONIFERE if "conifère" in bas else FEUILLU


def essence_bdforet(tfv, essence):
    """Niveau d'essence pour une formation de la BD Forêt, ou 0 si ce n'en est
       pas une (lande, formation herbacée : elles ne colorent rien, le contour
       vient de la BD TOPO)."""
    if not tfv.startswith("Forêt") and not tfv.startswith("Peupleraie"):
        return 0
    bas = essence.lower()
    if "mixte" in bas:
        return MIXTE
    if RE_PIN.search(bas):
        return PIN
    return CONIFERE if any(mot in bas for mot in MOTS_CONIFERE) else FEUILLU


def anneaux_de(geometry):
    """Renvoie les anneaux extérieurs (un par polygone) d'une géométrie Polygon
       ou MultiPolygon. Les trous sont ignorés : ces couches sont des pavages,
       un trou est comblé par une autre entité, que le tri par surface
       décroissante (voir peindre) dessine par-dessus."""
    if geometry is None:
        return []
    gtype = geometry["type"]
    coords = geometry["coordinates"]
    if gtype == "Polygon":
        return [coords[0]]
    if gtype == "MultiPolygon":
        return [poly[0] for poly in coords]
    return []


def lignes_de(geometry):
    """Renvoie les lignes d'une géométrie LineString ou MultiLineString."""
    if geometry is None:
        return []
    if geometry["type"] == "LineString":
        return [geometry["coordinates"]]
    if geometry["type"] == "MultiLineString":
        return geometry["coordinates"]
    return []


def epaisseur_route(chaussee):
    """Épaisseur du trait à gommer pour une chaussée donnée, en pixels : la
       chaussée elle-même (souvent absente sur les bretelles, d'où le repli) plus
       la marge dégagée de chaque côté."""
    largeur = max(float(chaussee or 0.0), CHAUSSEE_MIN_M) + 2.0 * MARGE_ROUTE_M
    return max(1, round(largeur / PX_M))


def surface(pts):
    """Surface (formule du lacet, valeur absolue) d'un anneau en pixels : sert à
       trier les polygones du plus grand au plus petit."""
    aire = 0.0
    for i in range(len(pts)):
        x0, y0 = pts[i - 1]
        x1, y1 = pts[i]
        aire += x0 * y1 - x1 * y0
    return abs(aire) * 0.5


def en_pixels(anneau, cadre):
    """Anneau (lon, lat) -> liste de pixels du masque. cadre vaut
       (lon_min, lon_max, lat_min, lat_max, cols, rows) : colonne 0 = ouest,
       rangée 0 = NORD, comme l'orthophoto (d'où le lat_max - lat)."""
    lon_min, lon_max, lat_min, lat_max, cols, rows = cadre
    return [((lon - lon_min) / (lon_max - lon_min) * (cols - 1),
             (lat_max - lat) / (lat_max - lat_min) * (rows - 1))
            for lon, lat, *_ in anneau]


def peindre(cadre, entites, niveau_de_feature):
    """Rastérise des entités WFS dans une image de niveaux de gris. niveau_de_feature
       rend le niveau à peindre pour une entité, ou 0 pour l'ignorer. Les polygones
       sont posés du plus grand au plus petit : une entité nichée dans le trou d'une
       autre est donc dessinée par-dessus, ce qui rend les trous sans les traiter."""
    image = Image.new("L", (cadre[4], cadre[5]), 0)
    dessin = ImageDraw.Draw(image)
    polygones = []
    for feat in entites:
        niveau = niveau_de_feature(feat["properties"])
        if niveau == 0:
            continue
        for anneau in anneaux_de(feat.get("geometry")):
            pts = en_pixels(anneau, cadre)
            if len(pts) >= 3:
                polygones.append((surface(pts), niveau, pts))
    polygones.sort(key=lambda p: p[0], reverse=True)
    for _, niveau, pts in polygones:
        dessin.polygon(pts, fill=niveau)
    return np.asarray(image), len(polygones)


def gommer(cadre, bbox):
    """Rastérise ce qui ne peut porter aucun arbre quoi qu'en dise la végétation :
       chaussées goudronnées, terrains de sport, pistes d'aérodrome. La BD TOPO
       cartographie les HAIES de bord de route ; large de 3 m, une haie gonfle d'un
       pixel à 10 m/px et déborde sur la chaussée, d'où des arbres sur l'A63. La
       parade est géométrique : on repasse par-dessus. Renvoie le raster de gomme
       et le compte par couche."""
    image = Image.new("L", (cadre[4], cadre[5]), 0)
    dessin = ImageDraw.Draw(image)

    routes = 0
    for feat in fetch_pages(bbox, WFS_LAYER_ROUTE, "nature,largeur_de_chaussee,geometrie",
                            filtre=filtre_natures(bbox, NATURES_ROUTE)):
        epaisseur = epaisseur_route(feat["properties"].get("largeur_de_chaussee"))
        for ligne in lignes_de(feat.get("geometry")):
            pts = en_pixels(ligne, cadre)
            if len(pts) >= 2:
                dessin.line(pts, fill=255, width=epaisseur, joint="curve")
                routes += 1

    surfaces = 0
    for couche in (WFS_LAYER_SPORT, WFS_LAYER_PISTE):
        for feat in fetch_pages(bbox, couche):
            for anneau in anneaux_de(feat.get("geometry")):
                pts = en_pixels(anneau, cadre)
                if len(pts) >= 3:
                    dessin.polygon(pts, fill=255)
                    surfaces += 1

    return np.asarray(image), routes, surfaces


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
