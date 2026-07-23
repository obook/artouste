"""
pilot.py
Repeint la tenue du pilote (combinaison et casque) à partir de l'atlas
d'origine du modèle (general_pilot.png). Contrairement au fuselage (métal gris
nuancé sur toute sa surface, voir retint.py), l'atlas du pilote n'a pas de zone
"métal neutre" à repeindre : la chemise et le casque sont repérés par leur
rectangle dans l'atlas, mesuré une fois sur general_pilot.png et stable tant
que le modèle n'est pas retexturé. Facteur commun aux trois tenues
(make_pilot_gendarmerie.py, make_pilot_armeedeterre.py,
make_pilot_protectioncivile.py).

Auteur : O. Booklage
Licence : GPL v2
"""

import numpy as np
from PIL import Image

# Rectangles de l'atlas (x0:x1, y0:y1, origine en haut à gauche) occupés par la
# chemise (combinaison) et le casque, mesurés sur general_pilot.png.
SHIRT_RECT = (200, 352, 283, 512)
HELMET_RECT = (400, 512, 0, 175)


def region_mask(mx, rect):
    """Masque booléen du rectangle donné, restreint aux pixels non blancs (le
       fond de l'atlas) : à l'intérieur du rectangle, seul le vêtement (non
       blanc) appartient au masque, le reste est du remplissage."""
    x0, x1, y0, y1 = rect
    m = np.zeros(mx.shape, dtype=bool)
    m[y0:y1, x0:x1] = True
    return m & (mx < 0.96)


def tint(out, lum, mask, target_hex, floor):
    """Teinte le masque donné dans la couleur target_hex (tuple RGB 0..255).

       La chemise et le casque sont des aplats presque unis (peu de dynamique
       de luminance) : un simple gain multiplicatif autour de la moyenne (comme
       pour le fuselage, voir retint.py) laisserait le casque quasi noir, sa
       luminance moyenne étant très basse. On étire donc la luminance du
       masque entre ses 1er et 99e centiles vers [floor, 1] avant de moduler la
       couleur cible : les creux gardent floor fois la teinte (jamais noir),
       les hautes lumières atteignent la teinte pleine."""
    target = np.array(target_hex, dtype=np.float32) / 255.0
    vals = lum[mask]
    lo, hi = np.percentile(vals, 1), np.percentile(vals, 99)
    norm = np.clip((lum - lo) / max(hi - lo, 1e-4), 0.0, 1.0)
    factor = floor + (1.0 - floor) * norm
    for c in range(3):
        out[..., c] = np.where(mask, target[c] * factor, out[..., c])
    return out


def make_pilot_outfit(src, dst, shirt_hex, helmet_hex):
    """Repeint la tenue (chemise + casque) de src vers dst."""
    img = np.asarray(Image.open(src).convert("RGB")).astype(np.float32) / 255.0
    r, g, b = img[..., 0], img[..., 1], img[..., 2]
    mx = np.maximum(np.maximum(r, g), b)
    lum = 0.299 * r + 0.587 * g + 0.114 * b

    shirt = region_mask(mx, SHIRT_RECT)
    helmet = region_mask(mx, HELMET_RECT)

    out = img.copy()
    out = tint(out, lum, shirt, shirt_hex, floor=0.30)
    out = tint(out, lum, helmet, helmet_hex, floor=0.50)

    out = np.clip(out, 0.0, 1.0)
    Image.fromarray((out * 255).astype("uint8")).save(dst)
    print(f"tenue pilote écrite -> {dst}  (chemise {shirt.sum()} px, casque {helmet.sum()} px)")
