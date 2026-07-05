#!/usr/bin/env python3
# Génère une livrée toute blanche à partir de l'atlas d'origine de l'Alouette II.
# On repeint en blanc uniquement les pixels neutres (le "métal" gris) et l'accent
# orange de la poutre, en laissant intacts les marquages saturés (cocardes
# tricolores). L'image d'origine n'est jamais modifiée : on écrit un fichier
# séparé. C'est le pendant blanc de make_gendarmerie.py, dont seule la couleur
# cible change.
import sys
from PIL import Image
import numpy as np

src = sys.argv[1] if len(sys.argv) > 1 else "assets/models/Alouette-II/Models/texture.png"
dst = sys.argv[2] if len(sys.argv) > 2 else "assets/models/Alouette-II/Models/texture-blanche.png"

img = np.asarray(Image.open(src).convert("RGB")).astype(np.float32) / 255.0
r, g, b = img[..., 0], img[..., 1], img[..., 2]

mx = np.maximum(np.maximum(r, g), b)
mn = np.minimum(np.minimum(r, g), b)
sat = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0.0)
lum = 0.299 * r + 0.587 * g + 0.114 * b

# Masque des pixels à repeindre :
#  - le "métal" gris, faiblement saturé ;
#  - l'accent orange de la poutre (un appareil tout blanc n'en a pas).
# On distingue l'orange du rouge de la cocarde : l'orange a un vert nettement
# supérieur au bleu (r > g > b), alors que le rouge de la cocarde a g proche de b.
neutral = sat < 0.18
orange  = (sat >= 0.18) & (r > g) & (g - b > 0.12) & (r - b > 0.25)
neutral = neutral | orange

# Blanc pur : #ffffff. On teinte chaque pixel neutre par cette couleur en conservant
# l'ombrage d'origine : le pixel de luminance moyenne devient blanc, les creux
# (lignes de tôle, recoins) restent un gris clair, les hautes lumières saturent en
# blanc. Le fuselage lit comme une peinture blanche, pas comme un aplat plat.
TARGET = np.array([0xff, 0xff, 0xff], dtype=np.float32) / 255.0
meanL = float(lum[neutral].mean())
gain = lum / meanL  # 1.0 pour un pixel de luminance moyenne -> couleur cible exacte

out = img.copy()
out[..., 0] = np.where(neutral, TARGET[0] * gain, r)
out[..., 1] = np.where(neutral, TARGET[1] * gain, g)
out[..., 2] = np.where(neutral, TARGET[2] * gain, b)

out = np.clip(out, 0.0, 1.0)
Image.fromarray((out * 255).astype("uint8")).save(dst)
print(f"livrée écrite -> {dst}  ({neutral.mean()*100:.0f}% des pixels repeints)")
