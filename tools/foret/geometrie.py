#!/usr/bin/env python3
"""
Nom du fichier : geometrie.py
Description : Anneaux, lignes, surfaces et passage en pixels du masque.
Auteur : O. Booklage
Date : Août 2026
Licence : GPL v2
"""

from foret.reglages import CHAUSSEE_MIN_M, MARGE_ROUTE_M, PX_M

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
