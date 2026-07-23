"""
retint.py
Repeint le fuselage de l'Alouette II (le "métal" gris et l'accent orange de la
poutre) dans une couleur cible, en conservant l'ombrage d'origine. Facteur
commun aux quatre livrées du fuselage (make_gendarmerie.py, make_blanche.py,
make_armeedeterre.py, make_protectioncivile.py), qui ne diffèrent que par leur
couleur cible et, pour la Protection civile, par un plafonnement du gain.

Auteur : O. Booklage
Licence : GPL v2
"""

import numpy as np
from PIL import Image


def retint(src, dst, target_hex, gain_clip=None):
    """Repeint src vers dst dans la couleur target_hex (tuple RGB 0..255).

       Masque des pixels à repeindre : le métal gris (faiblement saturé) et
       l'accent orange de la poutre, distingué du rouge de la cocarde (l'orange
       a un vert nettement supérieur au bleu, alors que le rouge de la cocarde a
       un vert proche du bleu). Le gain (luminance / luminance moyenne du
       masque) module la couleur cible pixel à pixel : la teinte moyenne des
       pixels repeints tombe exactement sur la cible, les creux deviennent plus
       sombres et les hautes lumières plus claires, sans changer de nuance.
       gain_clip, s'il est donné, borne ce gain (min, max) : sans borne haute,
       les reflets brillants du métal peuvent saturer un canal avant les
       autres et délaver la teinte (cas de la Protection civile, en rouge)."""
    img = np.asarray(Image.open(src).convert("RGB")).astype(np.float32) / 255.0
    r, g, b = img[..., 0], img[..., 1], img[..., 2]

    mx = np.maximum(np.maximum(r, g), b)
    mn = np.minimum(np.minimum(r, g), b)
    sat = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0.0)
    lum = 0.299 * r + 0.587 * g + 0.114 * b

    neutral = sat < 0.18
    orange = (sat >= 0.18) & (r > g) & (g - b > 0.12) & (r - b > 0.25)
    neutral = neutral | orange

    target = np.array(target_hex, dtype=np.float32) / 255.0
    mean_l = float(lum[neutral].mean())
    gain = lum / mean_l
    if gain_clip is not None:
        gain = np.clip(gain, gain_clip[0], gain_clip[1])

    out = img.copy()
    out[..., 0] = np.where(neutral, target[0] * gain, r)
    out[..., 1] = np.where(neutral, target[1] * gain, g)
    out[..., 2] = np.where(neutral, target[2] * gain, b)

    out = np.clip(out, 0.0, 1.0)
    Image.fromarray((out * 255).astype("uint8")).save(dst)
    print(f"livrée écrite -> {dst}  ({neutral.mean() * 100:.0f}% des pixels repeints)")
