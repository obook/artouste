#!/usr/bin/env python3
# Génère la tenue armée de terre (ALAT) du pilote à partir de l'atlas
# d'origine du modèle (general_pilot.png) : combinaison vert olive (même
# teinte que le fuselage, cf. make_armeedeterre.py) et casque kaki. Voir
# make_pilot_gendarmerie.py pour le principe (rectangles de l'atlas mesurés
# une fois sur general_pilot.png). L'image d'origine n'est jamais modifiée :
# on écrit un fichier séparé.
import sys
from PIL import Image
import numpy as np

src = sys.argv[1] if len(sys.argv) > 1 else "assets/models/Alouette-II/Models/Pilot/general_pilot.png"
dst = sys.argv[2] if len(sys.argv) > 2 else "assets/models/Alouette-II/Models/Pilot/general_pilot-armeedeterre.png"

img = np.asarray(Image.open(src).convert("RGB")).astype(np.float32) / 255.0
r, g, b = img[..., 0], img[..., 1], img[..., 2]
mx  = np.maximum(np.maximum(r, g), b)
lum = 0.299 * r + 0.587 * g + 0.114 * b

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
    # Voir make_pilot_gendarmerie.py : la chemise et le casque sont des aplats
    # presque unis, on étire donc la luminance du masque (1e-99e centile) vers
    # [floor, 1] plutôt qu'un simple gain autour de la moyenne (qui laisserait
    # le casque quasi noir, sa luminance moyenne étant très basse).
    target       = np.array(target_hex, dtype=np.float32) / 255.0
    vals         = lum[mask]
    lo, hi       = np.percentile(vals, 1), np.percentile(vals, 99)
    norm         = np.clip((lum - lo) / max(hi - lo, 1e-4), 0.0, 1.0)
    factor       = floor + (1.0 - floor) * norm
    for c in range(3):
        out[..., c] = np.where(mask, target[c] * factor, out[..., c])
    return out

# Vert armée (olive drab) #4b4e35, identique au fuselage (cf.
# make_armeedeterre.py). Casque kaki, plus clair que la combinaison.
out = img.copy()
out = tint(out, shirt, (0x4b, 0x4e, 0x35), floor=0.30)
out = tint(out, helmet, (0x8a, 0x7f, 0x52), floor=0.50)

out = np.clip(out, 0.0, 1.0)
Image.fromarray((out * 255).astype("uint8")).save(dst)
print(f"tenue pilote écrite -> {dst}  (chemise {shirt.sum()} px, casque {helmet.sum()} px)")
