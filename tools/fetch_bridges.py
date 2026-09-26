#!/usr/bin/env python3
"""
fetch_bridges.py
Télécharge les ouvrages d'art (ponts, viaducs, passages supérieurs) d'une carte
depuis la BD TOPO de l'IGN via le service WFS de la Géoplateforme, et les écrit
en fichier binaire compact (bridges.bin) que le moteur extrude en tabliers.

Pourquoi ce fichier : le relief est un modèle numérique de TERRAIN, le sol nu,
sans ouvrage d'art. Un pont n'existe donc que dans l'orthophoto plaquée dessus,
et son tablier suit le creux du lit du fleuve : il plonge dans l'eau. Mesuré sur
le pont d'Empalot à Toulouse, 8,5 m de creux entre la berge et le milieu de la
Garonne.

Ce que la BD TOPO apporte que le relief n'a pas : les tronçons de route et de
voie ferrée portent une géométrie en TROIS dimensions, dont l'altitude du
tablier, et un attribut position_par_rapport_au_sol qui vaut 1 ou plus quand la
voie est portée par un ouvrage.

Cette altitude ne s'emploie pas telle quelle. Mesuré sur cote-landes, 301
tronçons portés :

- 27 ont des points à -1000, la valeur "inconnue" de la BD TOPO, surtout
  au-dessus de l'eau ;
- ailleurs, un bout sur quatre tombe à plus d'un mètre du sol que dessine le
  moteur (précision annoncée : 1,5 m). Le tablier flottait au bout du pont ou
  s'enfonçait dans le remblai.

On recale donc chaque ouvrage sur le sol rendu, relevé au LiDAR HD, à ses bouts
LIBRES, ceux qui ne touchent pas un autre tronçon porté : là, la chaussée
rejoint son remblai. Le recalage se répartit entre les bouts par pondération
inverse du carré de la distance. Il vaut exactement l'écart mesuré à chaque
bout, et se raccorde de lui-même là où deux tronçons se touchent. Un ouvrage
sans altitude utilisable est posé sur ses seuls bouts.

On écarte aussi les tronçons que le relief porte déjà : ponceaux, dalots,
passages sur un fossé. Moins d'un mètre de creux sous le tablier, rien n'y est
écrasé, et un tablier posé sur le sol n'y ferait que scintiller.

Reste la largeur. La BD TOPO donne celle de la CHAUSSÉE, pas celle de
l'ouvrage : le pont Alexandre-III fait 40 m pour 18 m déclarés. Là où elle
dessine l'emprise du pont (couche construction_surfacique, nature "Pont", les
grands ouvrages), on mesure de chaque côté de l'axe la distance au bord de
l'emprise. Ailleurs, on estime un débord autour de la chaussée. Dans les deux
cas, un tronçon parallèle voisin (l'autre sens d'une autoroute, un trottoir
tracé à part) borne la largeur à mi-chemin : les tabliers se touchent sans se
chevaucher.

Format de bridges.bin (petit-boutiste) :
    magie   : 4 octets "ABRG"
    version : uint32 (= 2)
    nombre  : uint32 (nombre de tabliers)
    puis, pour chaque tablier :
        gauche   : float32 (demi-largeur de l'ouvrage à gauche de l'axe, mètres)
        droite   : float32 (idem à droite, dans le sens de parcours de l'axe)
        drapeaux : uint8   (bit 0 : photo seule, le pont est porté par un
                            monument 3D ; voir "ponts_des_monuments" dans la
                            zone de la carte)
        n        : uint16  (nombre de points de l'axe, au moins 2)
        n x (lon : float32, lat : float32, altitude : float32)

L'altitude écrite est celle de la chaussée, déjà recalée sur le sol du moteur.

Données : IGN Géoplateforme (BD TOPO, LiDAR HD), Licence Ouverte Etalab 2.0.

Usage : tools/.venv/bin/python tools/fetch_bridges.py <carte>
Sortie : assets/terrain/<carte>/bridges.bin

L'emprise est celle de terrain.txt, et non celle de la zone d'origine : dax et
cote-landes ont été recadrées, dax-arene n'a pas de zone.

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import os
import struct
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from foret.service_ign import fetch_pages, filtre_natures
from lidar.carte import M_PAR_DEG_LAT, flou_boite, sol_de_la_carte
from lidar.services import COUCHE_MNT, telecharger_relief
from terrain.config import NODATA, TERRAIN_ROOT
from terrain.meta import read_meta
from terrain.zones import ZONES

COUCHES = ("BDTOPO_V3:troncon_de_route", "BDTOPO_V3:troncon_de_voie_ferree")

# Largeur de repli quand la BD TOPO ne renseigne pas la chaussée (mètres).
# Sentiers et chemins n'en ont presque jamais : une passerelle, pas une route.
LARGEUR_ROUTE_DEFAUT = 6.0
LARGEUR_SENTIER = 2.5
NATURES_ETROITES = ("Sentier", "Escalier", "Chemin")

# Largeur d'une voie ferrée : l'entraxe usuel entre deux voies fait 4 m, et le
# ballast déborde de part et d'autre. La BD TOPO ne donne que le NOMBRE de
# voies, pas une largeur en mètres.
LARGEUR_PAR_VOIE = 4.5
LARGEUR_RAIL_MIN = 5.0

# En deçà, rien ne se voit d'en haut : escaliers, caillebotis.
LARGEUR_MIN = 2.0

# Creux minimal sous le tablier pour qu'il vaille la peine de le poser (m).
CREUX_MIN = 1.0

# Le sol se relève à cette distance au-delà d'un bout libre, sur la chaussée
# d'approche : au bout même, le MNT commence déjà à plonger sous l'ouvrage.
RECUL_BOUT_M = 4.0

# Marge du relevé LiDAR autour d'un tronçon : le recul du bout, plus la moitié
# du lissage de 16 m qui sépare le détail LiDAR du relief de la carte.
MARGE_M = 25.0

# Tolérance de simplification des axes (m). La BD TOPO pose un point tous les
# quelques mètres dans les courbes ; chaque segment coûte au shader du terrain,
# qui efface la photo sous les tabliers segment par segment. À 0,5 m, Toulouse
# passe d'environ 2300 segments à la moitié, sans écart visible sur un tablier.
TOLERANCE_AXE_M = 0.5

# Lissage à la maille de la carte : ce que la fenêtre de relief retranche du
# LiDAR avant de l'ajouter à la carte (MAILLE_CARTE_M, CalageRelief.hpp).
LISSAGE_CARTE_M = 16

# Débord de l'ouvrage de part et d'autre de la chaussée, faute d'emprise :
# bande d'arrêt, trottoir, parapet. Mesuré au nadir sur l'orthophoto au
# franchissement de la Garonne par l'A620 (pont d'Empalot) : 33 m de bitume en
# travers, pour deux tronçons déclarés à 13,5 m posés à 13 m d'entraxe, soit
# 27 m couverts. Il manquait 3 m d'un côté et 2,5 m de l'autre. Plafonné à 40 %
# de la chaussée : sans cela une passerelle de 3 m sortirait à 7 m de large.
DEBORD_M = 2.5
DEBORD_PART = 0.4

# Bornes d'une demi-largeur (m). Au-delà du maximum, l'emprise décrit une dalle
# ou un échangeur entier plutôt qu'un tablier.
DEMI_MIN_M = 1.0
DEMI_MAX_M = 40.0

# Un tronçon voisin borne la largeur s'il court à moins de 30 degrés de l'axe et
# à moins de cette distance.
PORTEE_VOISIN_M = 80.0
COS_PARALLELE = math.cos(math.radians(30.0))

# Points de l'axe où l'on mesure la largeur, en fraction de sa longueur : loin
# des bouts, où l'emprise s'évase sur les culées.
MESURES_LARGEUR = (0.25, 0.5, 0.75)

MAGIC = b"ABRG"
VERSION = 2


def filtre_portes(bbox):
    """Filtre OGC : dans l'emprise ET porté par un ouvrage. Le service ignore
       CQL_FILTER et refuse FILTER avec BBOX : l'emprise voyage dans le filtre,
       en LATITUDE LONGITUDE (voir foret/service_ign.py)."""
    lon_min, lon_max, lat_min, lat_max = bbox
    return (
        '<fes:Filter xmlns:fes="http://www.opengis.net/fes/2.0" '
        'xmlns:gml="http://www.opengis.net/gml/3.2"><fes:And>'
        '<fes:BBOX><fes:ValueReference>geometrie</fes:ValueReference>'
        '<gml:Envelope srsName="urn:ogc:def:crs:EPSG::4326">'
        f'<gml:lowerCorner>{lat_min} {lon_min}</gml:lowerCorner>'
        f'<gml:upperCorner>{lat_max} {lon_max}</gml:upperCorner>'
        '</gml:Envelope></fes:BBOX>'
        '<fes:PropertyIsGreaterThan>'
        '<fes:ValueReference>position_par_rapport_au_sol</fes:ValueReference>'
        '<fes:Literal>0</fes:Literal></fes:PropertyIsGreaterThan>'
        '</fes:And></fes:Filter>')


def largeur_de(proprietes):
    """Largeur de chaussée en mètres, d'après la route ou le nombre de voies."""
    if "largeur_de_chaussee" in proprietes:
        try:
            largeur = float(proprietes.get("largeur_de_chaussee"))
            if largeur > 0.0:
                return largeur
        except (TypeError, ValueError):
            pass
        if proprietes.get("nature") in NATURES_ETROITES:
            return LARGEUR_SENTIER
        return LARGEUR_ROUTE_DEFAUT
    try:
        voies = int(proprietes.get("nombre_de_voies"))
    except (TypeError, ValueError):
        voies = 1
    return max(LARGEUR_RAIL_MIN, LARGEUR_PAR_VOIE * max(1, voies))


def lignes_de(geometrie):
    """Liste des lignes d'une géométrie LineString ou MultiLineString."""
    if geometrie is None:
        return []
    if geometrie["type"] == "LineString":
        return [geometrie["coordinates"]]
    if geometrie["type"] == "MultiLineString":
        return geometrie["coordinates"]
    return []


def collecter(bbox):
    """Les tronçons portés de la zone : largeur et axe, altitude None si
       inconnue. "Gué ou radier" passe le filtre du serveur (comparaison de
       chaînes) : on ne garde que les niveaux numériques."""
    troncons = []
    for couche in COUCHES:
        for f in fetch_pages(None, couche, filtre=filtre_portes(bbox)):
            proprietes = f["properties"]
            if not str(proprietes.get("position_par_rapport_au_sol")).isdigit():
                continue
            largeur = largeur_de(proprietes)
            if largeur < LARGEUR_MIN:
                continue
            for ligne in lignes_de(f.get("geometry")):
                if len(ligne) < 2 or len(ligne) > 65535:
                    continue
                points = [(p[0], p[1], p[2] if len(p) > 2 and p[2] > NODATA + 1 else None)
                          for p in ligne]
                troncons.append({"largeur": largeur, "points": points})
        print(f"[ponts]   {couche} : {len(troncons)} tronçons portés")
    return troncons


class Releve:
    """Sol nu LiDAR autour d'un tronçon, à 1 m, et sa version lissée."""

    def __init__(self, points):
        lons = [p[0] for p in points]
        lats = [p[1] for p in points]
        self.m_lon = M_PAR_DEG_LAT * math.cos(math.radians(sum(lats) / len(lats)))
        self.lon0 = min(lons) - MARGE_M / self.m_lon
        self.lon1 = max(lons) + MARGE_M / self.m_lon
        self.lat0 = min(lats) - MARGE_M / M_PAR_DEG_LAT
        self.lat1 = max(lats) + MARGE_M / M_PAR_DEG_LAT
        self.w = max(8, int((self.lon1 - self.lon0) * self.m_lon))
        self.h = max(8, int((self.lat1 - self.lat0) * M_PAR_DEG_LAT))
        self.mnt = telecharger_relief(COUCHE_MNT, (self.lat0, self.lon0, self.lat1, self.lon1),
                                      self.w, self.h)
        # Hors couverture LiDAR, le service rend une sentinelle très négative :
        # rien à mesurer, le tronçon sera écarté.
        self.couvert = bool((self.mnt > NODATA + 1).all())
        self.lisse = flou_boite(self.mnt, LISSAGE_CARTE_M)

    def lire(self, grille, lon, lat):
        i = int((lon - self.lon0) / (self.lon1 - self.lon0) * self.w)
        j = int((self.lat1 - lat) / (self.lat1 - self.lat0) * self.h)
        return float(grille[min(self.h - 1, max(0, j)), min(self.w - 1, max(0, i))])


def cle(point):
    return (round(point[0], 7), round(point[1], 7))


def en_metres(lon, lat):
    """Plan local, assez juste pour pondérer des distances de quelques
       centaines de mètres."""
    return lon * M_PAR_DEG_LAT * math.cos(math.radians(lat)), lat * M_PAR_DEG_LAT


def composantes(troncons):
    """Groupes de tronçons qui se touchent par un bout (un ouvrage coupé à
       chaque intersection par la BD TOPO), et effectif de chaque bout."""
    parent = list(range(len(troncons)))

    def racine(k):
        while parent[k] != k:
            parent[k] = parent[parent[k]]
            k = parent[k]
        return k

    effectif, premier = {}, {}
    for k, t in enumerate(troncons):
        for bout in (t["points"][0], t["points"][-1]):
            c = cle(bout)
            effectif[c] = effectif.get(c, 0) + 1
            if c in premier:
                parent[racine(k)] = racine(premier[c])
            else:
                premier[c] = k
    groupes = {}
    for k in range(len(troncons)):
        groupes.setdefault(racine(k), []).append(k)
    return list(groupes.values()), effectif


def point_de_recul(bout, voisin, m_lon):
    """Point à RECUL_BOUT_M au-delà du bout, dans le prolongement de l'axe."""
    dx = (bout[0] - voisin[0]) * m_lon
    dy = (bout[1] - voisin[1]) * M_PAR_DEG_LAT
    n = math.hypot(dx, dy) or 1.0
    return (bout[0] + dx / n * RECUL_BOUT_M / m_lon,
            bout[1] + dy / n * RECUL_BOUT_M / M_PAR_DEG_LAT)


def sols_aux_bouts(carte, troncons, releves):
    """Pour chaque tronçon, le sol rendu au recul de ses deux bouts : c'est là
       qu'un bout libre s'appuie. Le sol que dessine le moteur près de l'appareil
       est le relief de la carte, plus le détail LiDAR que la fenêtre de relief y
       ajoute, c'est-à-dire le MNT moins son lissage à la maille de la carte."""
    reculs = []
    for t, r in zip(troncons, releves):
        pts = t["points"]
        reculs += [point_de_recul(pts[0], pts[1], r.m_lon), point_de_recul(pts[-1], pts[-2], r.m_lon)]
    cartes = sol_de_la_carte(carte, np.array([p[0] for p in reculs]), np.array([p[1] for p in reculs]))
    sols = []
    for k, r in enumerate(releves):
        sols.append(tuple(float(cartes[2 * k + b]) + r.lire(r.mnt, *reculs[2 * k + b])
                          - r.lire(r.lisse, *reculs[2 * k + b]) for b in (0, 1)))
    return sols


def ponderer(ancres, lon, lat):
    """Moyenne des valeurs d'ancrage pondérée par l'inverse du carré de la
       distance : exacte sur une ancre, continue partout ailleurs."""
    x, y = en_metres(lon, lat)
    somme = poids = 0.0
    for ax, ay, valeur in ancres:
        d2 = (x - ax) ** 2 + (y - ay) ** 2
        if d2 < 1e-4:
            return valeur
        somme += valeur / d2
        poids += 1.0 / d2
    return somme / poids


def recaler(troncons, sols):
    """Altitudes recalées de chaque tronçon (liste par tronçon), ou None pour un
       tronçon qu'on ne sait pas poser."""
    groupes, effectif = composantes(troncons)
    altitudes = [None] * len(troncons)
    for groupe in groupes:
        connue = all(p[2] is not None for k in groupe for p in troncons[k]["points"])
        ancres = []
        for k in groupe:
            pts = troncons[k]["points"]
            for bout, sol in zip((pts[0], pts[-1]), sols[k]):
                if effectif[cle(bout)] != 1:
                    continue  # bout partagé : pas un appui, le pont continue
                ancres.append((*en_metres(bout[0], bout[1]), sol - bout[2] if connue else sol))
        if not ancres and not connue:
            continue  # boucle sans appui ni altitude : rien de sûr à poser
        for k in groupe:
            altitudes[k] = [(p[2] if connue else 0.0) + (ponderer(ancres, p[0], p[1]) if ancres else 0.0)
                            for p in troncons[k]["points"]]
    return altitudes


def creux(troncon, altitudes, releve):
    """Plus grande hauteur du tablier au-dessus du sol nu, le long de l'axe."""
    pts = troncon["points"]
    plus = 0.0
    for i in range(len(pts) - 1):
        for t in np.linspace(0.0, 1.0, 9):
            lon = pts[i][0] + t * (pts[i + 1][0] - pts[i][0])
            lat = pts[i][1] + t * (pts[i + 1][1] - pts[i][1])
            z = altitudes[i] + t * (altitudes[i + 1] - altitudes[i])
            plus = max(plus, z - releve.lire(releve.mnt, lon, lat))
    return plus


def simplifier(points, m_lon):
    """Douglas-Peucker sur (x, y, z) en mètres : garde les points qui
       s'écartent de plus de TOLERANCE_AXE_M de la corde."""
    if len(points) < 3:
        return points
    xyz = [(p[0] * m_lon, p[1] * M_PAR_DEG_LAT, p[2]) for p in points]
    a, b = np.array(xyz[0]), np.array(xyz[-1])
    ab = b - a
    l2 = max(float(ab @ ab), 1e-9)
    ecarts = []
    for q in xyz[1:-1]:
        t = min(1.0, max(0.0, float((np.array(q) - a) @ ab) / l2))
        ecarts.append(float(np.linalg.norm(np.array(q) - (a + t * ab))))
    k = 1 + int(np.argmax(ecarts))
    if ecarts[k - 1] < TOLERANCE_AXE_M:
        return [points[0], points[-1]]
    return simplifier(points[:k + 1], m_lon)[:-1] + simplifier(points[k:], m_lon)


class Plan:
    """Plan local de la carte, en mètres : x vers l'est, y vers le nord."""

    def __init__(self, lon0, lat0):
        self.lon0, self.lat0 = lon0, lat0
        self.m_lon = M_PAR_DEG_LAT * math.cos(math.radians(lat0))

    def xy(self, lon, lat):
        return np.array([(lon - self.lon0) * self.m_lon, (lat - self.lat0) * M_PAR_DEG_LAT])


def emprises(bbox, plan):
    """Contours extérieurs des emprises "Pont" de la carte, en plan local."""
    lon_min, lon_max, lat_min, lat_max = bbox
    texte = f"{lon_min},{lat_min},{lon_max},{lat_max}"
    anneaux = []
    for f in fetch_pages(texte, "BDTOPO_V3:construction_surfacique",
                         filtre=filtre_natures(texte, ["Pont"])):
        geometrie = f.get("geometry") or {}
        if geometrie.get("type") == "Polygon":
            polygones = [geometrie["coordinates"]]
        elif geometrie.get("type") == "MultiPolygon":
            polygones = geometrie["coordinates"]
        else:
            continue
        for polygone in polygones:
            if polygone and len(polygone[0]) >= 4:
                anneaux.append(np.array([plan.xy(p[0], p[1]) for p in polygone[0]]))
    return anneaux


def traversees(p, d, a, b):
    """Distances t > 0 où la demi-droite p + t.d coupe les segments [a, b]."""
    e = b - a
    w = a - p
    den = d[0] * e[:, 1] - d[1] * e[:, 0]
    with np.errstate(divide="ignore", invalid="ignore"):
        t = (w[:, 0] * e[:, 1] - w[:, 1] * e[:, 0]) / den
        u = (w[:, 0] * d[1] - w[:, 1] * d[0]) / den
    return t[(np.abs(den) > 1e-12) & (u >= 0.0) & (u <= 1.0) & (t > 0.0)]


def bord_d_emprise(p, d, anneaux):
    """Distance de p au bord de l'emprise qui le contient, dans la direction d,
       ou None si p n'est dans aucune. Un nombre impair de traversées dit que p
       est dedans, même dans une emprise concave."""
    for anneau in anneaux:
        if not (anneau[:, 0].min() <= p[0] <= anneau[:, 0].max()
                and anneau[:, 1].min() <= p[1] <= anneau[:, 1].max()):
            continue
        t = traversees(p, d, anneau[:-1], anneau[1:])
        if len(t) % 2 == 1:
            return float(t.min())
    return None


def point_et_gauche(pts, fraction):
    """Point de l'axe à cette fraction de sa longueur, et la normale unitaire
       à sa gauche (sens de parcours)."""
    longueurs = [float(np.linalg.norm(pts[i + 1] - pts[i])) for i in range(len(pts) - 1)]
    reste = fraction * sum(longueurs)
    for i, l in enumerate(longueurs):
        if reste <= l or i == len(longueurs) - 1:
            d = (pts[i + 1] - pts[i]) / max(l, 1e-9)
            return pts[i] + d * min(reste, l), np.array([-d[1], d[0]])
        reste -= l


def demi_largeurs(pts, chaussee, anneaux, voisins):
    """Demi-largeurs (gauche, droite) de l'ouvrage. pts : axe en plan local ;
       voisins : segments (a, b) des autres tronçons parallèles proches."""
    estimee = 0.5 * chaussee + min(DEBORD_M, DEBORD_PART * chaussee)
    cotes = ([], [])
    for fraction in MESURES_LARGEUR:
        p, gauche = point_et_gauche(pts, fraction)
        # L'emprise ne donne la largeur que si elle s'allonge le long du
        # tronçon. Un raccord qui la traverse de biais ou en travers (carrefour
        # sur le pont Saint-Jean à Bordeaux) y mesurait sa longueur : une dalle
        # de 80 m.
        axe = np.array([gauche[1], -gauche[0]])
        mesures = [bord_d_emprise(p, d, anneaux) for d in (gauche, -gauche, axe, -axe)]
        dans_l_emprise = (None not in mesures
                          and mesures[2] + mesures[3] >= mesures[0] + mesures[1])
        for k, d in enumerate((gauche, -gauche)):
            demi = mesures[k] if dans_l_emprise else estimee
            if len(voisins[0]):
                t = traversees(p, d, voisins[0], voisins[1])
                if len(t):
                    demi = min(demi, 0.5 * float(t.min()))
            cotes[k].append(demi)
    return tuple(float(np.clip(np.median(c), DEMI_MIN_M, DEMI_MAX_M)) for c in cotes)


def largeurs(tabliers, anneaux, plan):
    """Demi-largeurs de chaque tablier (liste de (chaussée, points lon/lat/z))."""
    axes = [np.array([plan.xy(p[0], p[1]) for p in points]) for _, points in tabliers]
    segments = [(k, axe[i], axe[i + 1]) for k, axe in enumerate(axes) for i in range(len(axe) - 1)]
    resultat = []
    for k, (chaussee, _) in enumerate(tabliers):
        axe = axes[k]
        direction = axe[-1] - axe[0]
        direction = direction / max(float(np.linalg.norm(direction)), 1e-9)
        bas, haut = axe.min(0) - PORTEE_VOISIN_M, axe.max(0) + PORTEE_VOISIN_M
        a, b = [], []
        for j, sa, sb in segments:
            if j == k or not (np.all(np.minimum(sa, sb) <= haut) and np.all(np.maximum(sa, sb) >= bas)):
                continue
            e = sb - sa
            if abs(float(e @ direction)) >= COS_PARALLELE * float(np.linalg.norm(e)):
                a.append(sa)
                b.append(sb)
        voisins = (np.array(a).reshape(-1, 2), np.array(b).reshape(-1, 2))
        resultat.append(demi_largeurs(axe, chaussee, anneaux, voisins))
    return resultat


PHOTO_SEULE = 1


def portes_par_un_monument(carte, tabliers, plan):
    """Drapeaux de chaque tablier : PHOTO_SEULE si son milieu tombe dans un
       des cercles "ponts_des_monuments" de la zone."""
    cercles = [(plan.xy(lon, lat), rayon)
               for _, lon, lat, rayon in ZONES.get(carte, {}).get("ponts_des_monuments", [])]
    drapeaux = []
    for _, points in tabliers:
        axe = np.array([plan.xy(p[0], p[1]) for p in points])
        milieu = 0.5 * (axe.min(0) + axe.max(0))
        porte = any(np.linalg.norm(milieu - c) < r for c, r in cercles)
        drapeaux.append(PHOTO_SEULE if porte else 0)
    return drapeaux


def ecrire(chemin, tabliers, demis, drapeaux):
    with open(chemin, "wb") as out:
        out.write(MAGIC)
        out.write(struct.pack("<II", VERSION, len(tabliers)))
        for (_, points), (gauche, droite), drapeau in zip(tabliers, demis, drapeaux):
            out.write(struct.pack("<ffBH", gauche, droite, drapeau, len(points)))
            for lon, lat, alt in points:
                out.write(struct.pack("<fff", lon, lat, alt))


def main():
    if len(sys.argv) != 2:
        raise RuntimeError("usage : fetch_bridges.py <carte>")
    carte = sys.argv[1]
    meta = read_meta(Path(TERRAIN_ROOT) / carte / "terrain.txt")
    lon_min, lon_max = float(meta["lon_min"]), float(meta["lon_max"])
    lat_min, lat_max = float(meta["lat_min"]), float(meta["lat_max"])
    chemin = os.path.join(TERRAIN_ROOT, carte, "bridges.bin")
    debut = time.time()

    print(f"[ponts] carte {carte} : BD TOPO")
    troncons = collecter((lon_min, lon_max, lat_min, lat_max))
    sans_z = sum(1 for t in troncons if any(p[2] is None for p in t["points"]))

    print(f"[ponts] sol nu LiDAR autour de {len(troncons)} tronçons")
    with ThreadPoolExecutor(8) as pool:
        releves = list(pool.map(lambda t: Releve(t["points"]), troncons))
    sols = sols_aux_bouts(carte, troncons, releves)
    hors_lidar = sum(1 for r in releves if not r.couvert)

    # Deux passes : écarter un tronçon au ras du sol libère le bout de son
    # voisin, qui devient un appui et doit être recalé à son tour.
    garde = [k for k, r in enumerate(releves) if r.couvert]
    for _ in range(2):
        altitudes = recaler([troncons[k] for k in garde], [sols[k] for k in garde])
        garde = [k for k, alt in zip(garde, altitudes)
                 if alt is not None and creux(troncons[k], alt, releves[k]) >= CREUX_MIN]
    altitudes = recaler([troncons[k] for k in garde], [sols[k] for k in garde])

    tabliers = [(troncons[k]["largeur"],
                 simplifier([(p[0], p[1], z) for p, z in zip(troncons[k]["points"], alt)],
                            releves[k].m_lon))
                for k, alt in zip(garde, altitudes) if alt is not None]
    if not tabliers:
        print(f"[ponts] {len(troncons)} tronçons portés, aucun tablier à poser : rien d'écrit")
        return

    plan = Plan(0.5 * (lon_min + lon_max), 0.5 * (lat_min + lat_max))
    anneaux = emprises((lon_min, lon_max, lat_min, lat_max), plan)
    demis = largeurs(tabliers, anneaux, plan)
    drapeaux = portes_par_un_monument(carte, tabliers, plan)
    if any(drapeaux):
        print(f"[ponts] {sum(drapeaux)} tabliers laissés à un monument 3D (photo seule)")
    print(f"[ponts] {len(anneaux)} emprises de pont ; demi-largeur médiane "
          f"{np.median([g + d for g, d in demis]) / 2:.1f} m, la plus large "
          f"{max(g + d for g, d in demis):.0f} m")
    ecrire(chemin, tabliers, demis, drapeaux)
    print(f"[ponts] {len(troncons)} tronçons portés, dont {sans_z} sans altitude et "
          f"{hors_lidar} hors LiDAR ; {len(tabliers)} tabliers gardés (creux >= {CREUX_MIN} m)")
    print(f"[ponts] {chemin} écrit, {os.path.getsize(chemin) / 1024:.0f} ko, "
          f"en {time.time() - debut:.0f} s")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
