#!/usr/bin/env python3
"""
Nom du fichier : service_ign.py
Description : Requêtes au service WFS de la Géoplateforme, page par page.
Auteur : O. Booklage
Date : Août 2026
Licence : GPL v2
"""

import json
import time
import urllib.parse
import urllib.request

from foret.reglages import PAGE, WFS_URL

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
