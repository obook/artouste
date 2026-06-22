# -*- coding: utf-8 -*-
"""
ortho.py
Téléchargement de l'orthophoto (BD ORTHO) sur l'emprise de la zone via le service
WMS de l'IGN, puis nettoyage : comblement du no-data de bordure (montagne) ou
aplanissement de la mer en une couleur unie (bord de mer).

Auteur : O. Booklage
Licence : GPL v2
"""

import io
import os
import urllib.parse
import urllib.request

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

from terrain import config


def fill_nodata(arr):
    """Comble le no-data de la BD ORTHO (blanc pur en bordure de couverture) par une
       teinte de rocaille. On ne remplit que les grandes plages blanches connectées
       au bord de l'image : la neige des sommets (blanche mais à l'intérieur) est
       épargnée."""
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
    rock = np.median(arr[~white], axis=0) * 0.9  # teinte du terrain, assombrie
    out = arr.copy()
    out[nodata] = rock.astype(np.uint8)
    print(f"[ortho] no-data comblé ({int(nodata.sum())} px) couleur {rock.round().astype(int).tolist()}")
    return out


def fetch_ortho(aspect):
    """Télécharge l'orthophoto sur la même emprise (haut = nord) et recolore la mer."""
    width = int(round(config.ORTHO_HEIGHT * aspect))
    print(f"[ortho] WMS {width}x{config.ORTHO_HEIGHT} sur {config.WMS_LAYER}...")
    query = urllib.parse.urlencode({
        "SERVICE": "WMS",
        "VERSION": "1.3.0",
        "REQUEST": "GetMap",
        "LAYERS": config.WMS_LAYER,
        "STYLES": "",
        "CRS": "EPSG:4326",            # axes lat,lon en WMS 1.3.0
        "BBOX": f"{config.LAT_MIN},{config.LON_MIN},{config.LAT_MAX},{config.LON_MAX}",
        "WIDTH": width,
        "HEIGHT": config.ORTHO_HEIGHT,
        "FORMAT": "image/jpeg",
    })
    url = config.WMS_URL + "?" + query
    with urllib.request.urlopen(url, timeout=120) as resp:
        content = resp.read()
    if content[:2] != b"\xff\xd8":  # pas un JPEG : c'est une erreur du service
        raise RuntimeError("le WMS n'a pas renvoyé une image : "
                           + content[:200].decode("utf-8", "replace"))
    path = os.path.join(config.OUT_DIR, "ortho.jpg")

    # Zone sans mer (montagne) : pas de recoloration de la mer (qui bleuirait la
    # neige) ; on comble seulement le no-data de la BD ORTHO par de la rocaille.
    if not config.RECOLOR_SEA:
        arr = np.array(Image.open(io.BytesIO(content)).convert("RGB"))
        Image.fromarray(fill_nodata(arr)).save(path, quality=88)
        print(f"[ortho] {path} écrit ({width}x{config.ORTHO_HEIGHT})")
        return width

    raw = os.path.join(config.OUT_DIR, "_ortho_raw.jpg")
    with open(raw, "wb") as out:
        out.write(content)

    # La mer de la BD ORTHO pose deux problèmes : au large elle revient en blanc
    # (sans donnée), et l'eau photographiée est une mosaïque de dalles aux teintes
    # différentes, dont les bords en escalier formeraient des "pavés" au rendu.
    # On aplanit donc toute la mer à une couleur unie, en préservant l'écume
    # blanche et la plage (non bleutées) pour garder un trait de côte net.
    arr = np.array(Image.open(raw).convert("RGB")).astype(np.float32)
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

    Image.fromarray(out.clip(0, 255).astype(np.uint8)).save(path, quality=88)
    os.remove(raw)
    print(f"[ortho] {path} écrit ({width}x{config.ORTHO_HEIGHT})")
    return width
