#!/usr/bin/env python3
"""
Nom du fichier : masque.py
Description : Peinture du masque de forêt, puis gomme des routes et terrains.
Auteur : O. Booklage
Date : Août 2026
Licence : GPL v2
"""

import numpy as np
from PIL import Image, ImageDraw

from foret.geometrie import (anneaux_de, en_pixels, epaisseur_route,
                             lignes_de, surface)
from foret.reglages import (NATURES_ROUTE, WFS_LAYER_PISTE, WFS_LAYER_ROUTE,
                            WFS_LAYER_SPORT)
from foret.service_ign import fetch_pages, filtre_natures

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
