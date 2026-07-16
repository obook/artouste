#!/usr/bin/env python3
# Génère la tenue Gendarmerie du pilote à partir de l'atlas d'origine du modèle
# (general_pilot.png) : combinaison bleu gendarmerie (même teinte que le
# fuselage, cf. make_gendarmerie.py) et casque blanc. Contrairement aux
# livrées du fuselage, l'atlas du pilote n'a pas de zone "métal neutre" à
# repeindre : la chemise et le casque sont repérés par leur rectangle dans
# l'atlas (mesuré une fois sur general_pilot.png, stable tant que le modèle
# n'est pas retexturé). L'image d'origine n'est jamais modifiée : on écrit un
# fichier séparé.
import sys
from PIL import Image
import numpy as np

src = sys.argv[1] if len(sys.argv) > 1 else "assets/models/Alouette-II/Models/Pilot/general_pilot.png"
dst = sys.argv[2] if len(sys.argv) > 2 else "assets/models/Alouette-II/Models/Pilot/general_pilot-gendarmerie.png"

img = np.asarray(Image.open(src).convert("RGB")).astype(np.float32) / 255.0
r, g, b = img[..., 0], img[..., 1], img[..., 2]
mx  = np.maximum(np.maximum(r, g), b)
lum = 0.299 * r + 0.587 * g + 0.114 * b

# Rectangles de l'atlas (x0:x1, y0:y1, origine en haut à gauche) occupés par la
# chemise (combinaison) et le casque, mesurés sur general_pilot.png. À
# l'intérieur de chaque rectangle, seuls les pixels non blancs (le fond de
# l'atlas) appartiennent au vêtement : le reste du rectangle est du remplissage.
SHIRT_RECT  = (200, 352, 283, 512)
HELMET_RECT = (400, 512, 0, 175)

def region_mask(rect):
    x0, x1, y0, y1 = rect
    m = np.zeros(mx.shape, dtype=bool)
    m[y0:y1, x0:x1] = True
    return m & (mx < 0.96)

shirt  = region_mask(SHIRT_RECT)
helmet = region_mask(HELMET_RECT)

def tint(out, mask, target_hex, floor):
    # Contrairement au fuselage (métal gris nuancé sur toute sa surface), la
    # chemise et le casque sont des aplats presque unis (peu de dynamique de
    # luminance) : un simple gain multiplicatif autour de la moyenne laisse le
    # casque quasi noir (sa luminance moyenne est très basse). On étire donc la
    # luminance du masque entre ses 1e et 99e centiles vers [floor, 1] avant de
    # moduler la couleur cible : les creux gardent 'floor' x la teinte (jamais
    # noir), les hautes lumières atteignent la teinte pleine.
    target       = np.array(target_hex, dtype=np.float32) / 255.0
    vals         = lum[mask]
    lo, hi       = np.percentile(vals, 1), np.percentile(vals, 99)
    norm         = np.clip((lum - lo) / max(hi - lo, 1e-4), 0.0, 1.0)
    factor       = floor + (1.0 - floor) * norm
    for c in range(3):
        out[..., c] = np.where(mask, target[c] * factor, out[..., c])
    return out

# Bleu gendarmerie #374f6b, identique au fuselage (cf. make_gendarmerie.py).
# Casque blanc (la visière fumée n'est pas modélisée séparément dans l'atlas).
out = img.copy()
out = tint(out, shirt, (0x37, 0x4f, 0x6b), floor=0.30)
out = tint(out, helmet, (0xff, 0xff, 0xff), floor=0.50)

out = np.clip(out, 0.0, 1.0)
Image.fromarray((out * 255).astype("uint8")).save(dst)
print(f"tenue pilote écrite -> {dst}  (chemise {shirt.sum()} px, casque {helmet.sum()} px)")
