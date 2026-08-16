"""
services.py
Accès aux couches LiDAR HD et BD ORTHO de l'IGN, servies en raster par le même
service WMS que le relief des cartes. Le nuage de points brut n'est jamais
nécessaire : l'IGN publie déjà le sol nu (MNT), la surface (MNS) et la hauteur
de sursol (MNH) sous forme de grilles.

Auteur : O. Booklage
Licence : GPL v2
"""

import time
import urllib.parse
import urllib.request
import io

import numpy as np
from PIL import Image

from terrain import config

# Produits dérivés du LiDAR HD, en BIL flottant 32 bits comme le RGE ALTI.
COUCHE_MNT = "IGNF_LIDAR-HD_MNT_ELEVATION.ELEVATIONGRIDCOVERAGE.WGS84G"
COUCHE_MNS = "IGNF_LIDAR-HD_MNS_ELEVATION.ELEVATIONGRIDCOVERAGE.WGS84G"
COUCHE_MNH = "IGNF_LIDAR-HD_MNH_ELEVATION.ELEVATIONGRIDCOVERAGE.WGS84G"

# Finesse native de la BD ORTHO ; en deçà il n'y a rien à gagner.
ORTHO_M_PAR_PX = 0.20


def _lire_url(url, timeout=240, essais=4):
    """Une requête HTTP, retentée : le service IGN est parfois capricieux."""
    dernier = None
    for essai in range(essais):
        try:
            with urllib.request.urlopen(url, timeout=timeout) as reponse:
                return reponse.read()
        except Exception as err:
            dernier = err
            time.sleep(2.0 * (essai + 1))
    raise RuntimeError(f"requête WMS : échec après {essais} essais ({dernier})")


def telecharger_relief(couche, bbox, largeur, hauteur):
    """Une requête de relief, rendue en tableau de mètres (rangée 0 = nord).

       bbox = (lat_min, lon_min, lat_max, lon_max), ordre des axes du WMS 1.3.0."""
    requete = urllib.parse.urlencode({
        "SERVICE": "WMS",
        "VERSION": "1.3.0",
        "REQUEST": "GetMap",
        "LAYERS": couche,
        "STYLES": "",
        "CRS": "EPSG:4326",
        "BBOX": "%.8f,%.8f,%.8f,%.8f" % bbox,
        "WIDTH": largeur,
        "HEIGHT": hauteur,
        "FORMAT": config.ALTI_WMS_FORMAT,
    })
    donnees = _lire_url(config.WMS_URL + "?" + requete)
    attendu = largeur * hauteur * 4
    if len(donnees) != attendu:
        # Le service répond en XML quand il refuse : on le remonte tel quel.
        raise RuntimeError(f"{couche} : {len(donnees)} octets reçus au lieu de {attendu} "
                           f"({donnees[:160].decode('utf-8', 'replace')})")
    return np.frombuffer(donnees, dtype="<f4").reshape(hauteur, largeur).astype(np.float64)


def telecharger_ortho(bbox, largeur, hauteur):
    """L'orthophoto de l'emprise, en image RGB (rangée 0 = nord)."""
    requete = urllib.parse.urlencode({
        "SERVICE": "WMS",
        "VERSION": "1.3.0",
        "REQUEST": "GetMap",
        "LAYERS": config.WMS_LAYER,
        "STYLES": "",
        "CRS": "EPSG:4326",
        "BBOX": "%.8f,%.8f,%.8f,%.8f" % bbox,
        "WIDTH": largeur,
        "HEIGHT": hauteur,
        "FORMAT": "image/jpeg",
    })
    return Image.open(io.BytesIO(_lire_url(config.WMS_URL + "?" + requete))).convert("RGB")



def grille_altitudes(couche, bornes, nx, nz, surech=4, journal=True):
    """Grille d'altitudes aux NOEUDS d'une carte, en mètres (rangée 0 = nord).

       bornes = (lon_min, lon_max, lat_min, lat_max) des noeuds extrêmes.

       Deux précautions, reprises de terrain/relief.py :

       - calage : nos points sont des NOEUDS, le premier sur la bordure ouest,
         alors que le service rend des PIXELS dont les centres tombent à un
         demi-pas à l'intérieur. On élargit donc l'emprise d'un demi-pas ;
       - suréchantillonnage : le service rééchantillonne au plus proche depuis
         sa propre pyramide. On demande surech fois plus de pixels que de
         points et on moyenne soi-même, ce qui rend l'altitude MOYENNE de la
         maille au lieu d'un point qui peut tomber sur une arête.

       La demande est découpée en blocs pour rester sous la limite du service."""
    lon_min, lon_max, lat_min, lat_max = bornes
    pas_lon = (lon_max - lon_min) / (nx - 1)
    pas_lat = (lat_max - lat_min) / (nz - 1)
    grille = np.zeros((nz, nx), dtype=np.float64)

    par_bloc = max(1, config.WMS_MAX_PX // surech)
    blocs_x = list(range(0, nx, par_bloc))
    blocs_y = list(range(0, nz, par_bloc))
    if journal:
        print(f"[relief] grille {nx}x{nz} sur {couche.split('_')[1]}, "
              f"suréchantillonnage x{surech}, "
              f"{len(blocs_x) * len(blocs_y)} requête(s), "
              f"{nx * nz * surech * surech * 4 / 1e6:.0f} Mo à recevoir")

    fait = 0
    for j0 in blocs_y:
        j1 = min(j0 + par_bloc, nz)
        for i0 in blocs_x:
            i1 = min(i0 + par_bloc, nx)
            bbox = (lat_max - (j1 - 1) * pas_lat - pas_lat / 2.0,
                    lon_min + i0 * pas_lon - pas_lon / 2.0,
                    lat_max - j0 * pas_lat + pas_lat / 2.0,
                    lon_min + (i1 - 1) * pas_lon + pas_lon / 2.0)
            brut = telecharger_relief(couche, bbox, (i1 - i0) * surech, (j1 - j0) * surech)
            if surech == 1:
                grille[j0:j1, i0:i1] = brut
            else:
                # Les points sans donnée valent une sentinelle très négative :
                # les moyenner avec leurs voisins valides donnerait n'importe quoi.
                valide = np.where(brut > config.NODATA, brut, np.nan)
                paquets = valide.reshape(j1 - j0, surech, i1 - i0, surech)
                with np.errstate(invalid="ignore"):
                    moyenne = np.nanmean(paquets, axis=(1, 3))
                grille[j0:j1, i0:i1] = np.where(np.isnan(moyenne), config.NODATA - 1.0, moyenne)
            fait += 1
            if journal:
                print(f"[relief]   {fait} bloc(s), {i1 - i0}x{j1 - j0} points")
    return grille
