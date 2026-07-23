#!/usr/bin/env python3
"""
make_puff.py
Génère une bouffée de nuage pour les nuages en billboards (prototype).
Plutôt qu'un disque parfait (qui donne des nuages en boule), la bouffée est un
AMAS DE LOBES : plusieurs bosses douces réunies, ce qui donne un contour irrégulier
et bosselé, façon chou-fleur de cumulus. La couleur reste blanche (le shader ombre
la bouffée selon le soleil et sa hauteur dans le nuage).

Assets PROCÉDURAUX du prototype (aucune donnée externe, aucune question de licence).

Usage : python3 tools/clouds/make_puff.py
Sortie : assets/clouds/puff.png (256x256, RGBA)

Auteur : O. Booklage
Licence : GPL v2
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/
from common.paths import assets_dir

N = 256


def main():
    rng = np.random.default_rng(1789)  # déterministe

    yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
    alpha = np.zeros((N, N), np.float32)

    # Amas de lobes : chaque lobe est une bosse douce (chute quadratique). L'union
    # (maximum) de lobes décalés donne un contour bosselé, pas un cercle net. Les
    # lobes sont un peu tassés vers le bas (base d'un peu plus lourde qu'un ballon).
    n_lobes = 9
    for i in range(n_lobes):
        cx = N * (0.5 + rng.uniform(-0.22, 0.22))
        cy = N * (0.52 + rng.uniform(-0.16, 0.24))   # léger biais vers le bas
        rad = N * rng.uniform(0.16, 0.30)
        d2 = ((xx - cx) ** 2 + (yy - cy) ** 2) / (rad * rad)
        lobe = np.clip(1.0 - d2, 0.0, 1.0)           # 1 au centre, 0 au bord
        lobe = lobe * lobe                            # bord plus doux
        alpha = np.maximum(alpha, lobe)

    # Grain interne (le coeur n'est pas parfaitement uni).
    noise = rng.random((N, N)).astype(np.float32)
    noise_img = Image.fromarray((noise * 255).astype(np.uint8)).filter(
        ImageFilter.GaussianBlur(N * 0.05))
    noise = np.asarray(noise_img).astype(np.float32) / 255.0
    alpha *= 0.82 + 0.20 * noise

    a = np.clip(alpha, 0.0, 1.0)
    rgb = np.full((N, N, 3), 255, dtype=np.uint8)
    img = np.dstack([rgb, (a * 255).astype(np.uint8)[..., None]])
    out = Image.fromarray(img, "RGBA").filter(ImageFilter.GaussianBlur(1.2))

    out_path = assets_dir("clouds", "puff.png")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out.save(out_path)
    print("[clouds] bouffée écrite : {} ({}x{})".format(out_path, N, N))


if __name__ == "__main__":
    main()
