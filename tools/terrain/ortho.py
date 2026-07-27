"""
ortho.py
Téléchargement de l'orthophoto (BD ORTHO) sur l'emprise de la zone via le service
WMS de l'IGN, puis nettoyage : comblement du no-data de bordure (montagne) ou
aplanissement de la mer en une couleur unie (bord de mer).

Auteur : O. Booklage
Licence : GPL v2
"""

import io
import math
import os
import time
import urllib.parse
import urllib.request

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

from terrain import config


def fill_nodata(arr):
    """Comble le no-data de la BD ORTHO (blanc pur en bordure de couverture, la
       frontière espagnole sur les zones de montagne) en PROLONGEANT le paysage :
       chaque pixel manquant prend la couleur du pixel valide le plus proche,
       puis la zone comblée est adoucie au flou pour fondre les traînées de
       prolongation. L'ancienne teinte unie dessinait de gros carrés plats,
       criards à côté des névés. On ne remplit que les grandes plages blanches
       connectées au bord de l'image : la neige des sommets (blanche mais à
       l'intérieur) est épargnée."""
    white = (arr[:, :, 0] >= 248) & (arr[:, :, 1] >= 248) & (arr[:, :, 2] >= 248)
    labels, count = ndimage.label(white)
    if count == 0:
        return arr
    border = set(labels[0, :]) | set(labels[-1, :]) | set(labels[:, 0]) | set(labels[:, -1])
    border.discard(0)
    sizes = ndimage.sum(np.ones_like(labels), labels, index=range(1, count + 1))
    keep = [lab for lab in border if sizes[lab - 1] > 3000]
    nodata = np.isin(labels, keep)
    if not nodata.any():
        return arr
    # Prolongation : chaque pixel manquant prend la couleur du pixel valide le
    # plus proche, puis un GRAND flou lisse l'intérieur comblé (sans lui, les
    # couleurs prolongées gardaient la géométrie en escalier des tuiles WMS,
    # flagrante sur la minimap).
    out = arr.copy()
    _, (iy, ix) = ndimage.distance_transform_edt(nodata, return_indices=True)
    out[nodata] = arr[iy[nodata], ix[nodata]]
    flou = np.asarray(Image.fromarray(out).filter(ImageFilter.GaussianBlur(48)),
                      dtype=np.float32)
    # Fondu progressif de part et d'autre de la frontière de couverture (rampe
    # d'environ 24 px) : la photo reste intacte au-delà, le comblement flou prend
    # le dessus au coeur, et la marche d'escalier disparaît.
    dedans = ndimage.distance_transform_edt(nodata)
    dehors = ndimage.distance_transform_edt(~nodata)
    alpha = np.clip(0.5 + (dedans - dehors) / 24.0, 0.0, 1.0)[:, :, np.newaxis]
    out = (arr.astype(np.float32) * (1.0 - alpha) + flou * alpha).astype(np.uint8)
    print(f"[ortho] no-data comblé ({int(nodata.sum())} px) par prolongation fondue du paysage")
    return out


def _request_tile(lat_lo, lon_lo, lat_hi, lon_hi, width, height):
    """Une requête WMS GetMap sur une emprise donnée, avec quelques essais (réseau
       capricieux ou 429 du serveur IGN)."""
    query = urllib.parse.urlencode({
        "SERVICE": "WMS",
        "VERSION": "1.3.0",
        "REQUEST": "GetMap",
        "LAYERS": config.WMS_LAYER,
        "STYLES": "",
        "CRS": "EPSG:4326",            # axes lat,lon en WMS 1.3.0
        "BBOX": f"{lat_lo},{lon_lo},{lat_hi},{lon_hi}",
        "WIDTH": width,
        "HEIGHT": height,
        "FORMAT": "image/jpeg",
    })
    url = config.WMS_URL + "?" + query
    last_err = None
    for attempt in range(4):
        try:
            with urllib.request.urlopen(url, timeout=120) as resp:
                content = resp.read()
            if content[:2] != b"\xff\xd8":  # pas un JPEG : erreur du service
                raise RuntimeError("le WMS n'a pas renvoyé une image : "
                                   + content[:200].decode("utf-8", "replace"))
            return Image.open(io.BytesIO(content)).convert("RGB")
        except Exception as err:
            last_err = err
            time.sleep(2.0 * (attempt + 1))
    raise RuntimeError(f"tuile WMS {width}x{height} : échec après plusieurs essais ({last_err})")


def request_map(lat_lo, lon_lo, lat_hi, lon_hi, width, height):
    """Une requête WMS GetMap, exposée aux outils qui composent eux-mêmes leur
       mosaïque au lieu de laisser fetch_ortho l'assembler en mémoire : c'est le
       cas du découpage en tuiles de détail, dont la mosaïque complète tiendrait
       des centaines de mégapixels (voir fetch_tuiles.py)."""
    return _request_tile(lat_lo, lon_lo, lat_hi, lon_hi, width, height)


def _tile_edges(total_px, max_px):
    """Découpe [0, total_px) en tranches contiguës d'au plus max_px pixels."""
    n = math.ceil(total_px / max_px)
    return [round(i * total_px / n) for i in range(n + 1)]


def _fetch_image(width, height):
    """Télécharge l'ortho sur toute l'emprise de la zone (haut = nord), en une seule
       requête WMS si les dimensions tiennent sous la limite serveur, sinon en
       assemblant une mosaïque de tuiles (limite dépassée sur une carte "pleine
       taille" comme dax quand on vise une résolution plus fine que la limite ne
       permet en une seule requête)."""
    if width <= config.WMS_MAX_PX and height <= config.WMS_MAX_PX:
        print(f"[ortho] WMS {width}x{height} sur {config.WMS_LAYER}...")
        return _request_tile(config.LAT_MIN, config.LON_MIN, config.LAT_MAX, config.LON_MAX,
                             width, height)

    x_edges = _tile_edges(width, config.WMS_MAX_PX)
    y_edges = _tile_edges(height, config.WMS_MAX_PX)
    n_tiles = (len(x_edges) - 1) * (len(y_edges) - 1)
    print(f"[ortho] WMS {width}x{height} sur {config.WMS_LAYER} "
          f"(mosaïque {len(x_edges) - 1}x{len(y_edges) - 1} tuiles)...")
    canvas = Image.new("RGB", (width, height))
    done = 0
    for y_lo, y_hi in zip(y_edges[:-1], y_edges[1:]):
        lat_hi = config.LAT_MAX - y_lo / height * (config.LAT_MAX - config.LAT_MIN)
        lat_lo = config.LAT_MAX - y_hi / height * (config.LAT_MAX - config.LAT_MIN)
        for x_lo, x_hi in zip(x_edges[:-1], x_edges[1:]):
            lon_lo = config.LON_MIN + x_lo / width * (config.LON_MAX - config.LON_MIN)
            lon_hi = config.LON_MIN + x_hi / width * (config.LON_MAX - config.LON_MIN)
            tile = _request_tile(lat_lo, lon_lo, lat_hi, lon_hi, x_hi - x_lo, y_hi - y_lo)
            canvas.paste(tile, (x_lo, y_lo))
            done += 1
            print(f"[ortho]   tuile {done}/{n_tiles}")
    return canvas


def flatten_sea(img):
    """Aplanit la mer à une couleur unie, en préservant l'écume blanche et la
       plage (non bleutées) pour garder un trait de côte net. La mer de la BD
       ORTHO pose deux problèmes : au large elle revient en blanc (sans
       donnée), et l'eau photographiée est une mosaïque de dalles aux teintes
       différentes, dont les bords en escalier formeraient des "pavés" au
       rendu. Renvoie l'image (RGB, uint8) aplanie."""
    arr = np.array(img).astype(np.float32)
    r, g, b = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
    # Le blanc "sans donnée" de la BD ORTHO est l'océan au large, hors couverture :
    # une grande plage blanche qui touche les bords de l'image. Le sable très clair
    # (la dune du Pilat) est blanc lui aussi, mais à l'intérieur des terres ; on ne
    # retient donc comme mer que le blanc relié au bord, sinon la dune serait bleuie.
    white = (r > 250) & (g > 250) & (b > 250)
    labels, n_white = ndimage.label(white)
    nodata = np.zeros_like(white)
    if n_white:
        border = set(labels[0, :]) | set(labels[-1, :]) | set(labels[:, 0]) | set(labels[:, -1])
        border.discard(0)
        if border:
            nodata = np.isin(labels, list(border))
    water = (b > r + 2) & (b > g - 4) & (arr.max(axis=2) < 155)  # eau bleutée, pas l'écume
    sea = nodata | water

    photographed = water & ~nodata
    deep = np.median(arr[photographed], axis=0) if int(photographed.sum()) > 1000 \
        else np.array(config.SEA_FALLBACK, dtype=np.float32)

    out = arr.copy()
    out[nodata] = deep
    alpha = np.asarray(Image.fromarray((sea * 255).astype(np.uint8))
                       .filter(ImageFilter.GaussianBlur(4)), dtype=np.float32) / 255.0
    alpha = alpha[:, :, None]
    out = out * (1.0 - alpha) + deep * alpha
    print(f"[ortho] mer aplanie ({int(sea.sum())} px) couleur {deep.round().astype(int).tolist()}")
    return out.clip(0, 255).astype(np.uint8)


def fetch_ortho(aspect):
    """Télécharge l'orthophoto sur la même emprise (haut = nord) et recolore la mer."""
    width = int(round(config.ORTHO_HEIGHT * aspect))
    height = config.ORTHO_HEIGHT
    img = _fetch_image(width, height)
    path = os.path.join(config.OUT_DIR, "ortho.jpg")

    # Zone sans mer (montagne) : pas de recoloration de la mer (qui bleuirait la
    # neige) ; on comble seulement le no-data de la BD ORTHO par de la rocaille.
    if not config.RECOLOR_SEA:
        arr = np.array(img)
        Image.fromarray(fill_nodata(arr)).save(path, quality=config.ORTHO_JPEG_QUALITY)
        print(f"[ortho] {path} écrit ({width}x{height})")
        return width

    Image.fromarray(flatten_sea(img)).save(path, quality=config.ORTHO_JPEG_QUALITY)
    print(f"[ortho] {path} écrit ({width}x{height})")
    return width
