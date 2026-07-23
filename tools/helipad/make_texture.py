#!/usr/bin/env python3
"""
make_texture.py
Génère la texture de l'hélistation : béton usé et sali, cercle de poser et grand
H blanc (marquage d'hélistation civile, sans croix). L'image est carrée ; le
disque de l'helipad y est inscrit (les coins, hors disque, ne se voient pas).

À lancer avec le Python du venv du projet (voir tools/.venv) :
    tools/.venv/bin/python tools/helipad/make_texture.py assets/models/helipad/helipad.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/
from common import imaging

SIZE = 2048  # côté de l'image, en pixels


def concrete():
    """Dalle de béton : gris moyen, grain à deux échelles, plus un assombrissement
    vers le bord (salissure)."""
    base = Image.new("RGB", (SIZE, SIZE), (146, 147, 150))
    fine = imaging.noise(SIZE, SIZE, 34, 1.0)
    coarse = imaging.noise(SIZE, SIZE, 60, 9.0)
    grain = ImageChops.multiply(fine, coarse).point(lambda p: 110 + p // 3)
    grain_rgb = Image.merge("RGB", (grain, grain, grain))
    img = Image.blend(base, grain_rgb, 0.45)
    return img


def worn_alpha(base_alpha):
    """Masque d'alpha d'une peinture usée : surtout opaque, avec un grain fin
    d'éraflures et de grandes plages où la peinture s'est effacée (le béton
    réapparaît par endroits), pour un marquage vieilli plutôt que neuf."""
    # Grain fin : petites éraflures, peinture presque intacte en général.
    fine = imaging.noise(SIZE, SIZE, 70, 2.0).point(
        lambda p: base_alpha if p > 64 else base_alpha - 70 + p)
    # Grandes plages d'usure : flou modéré (pour garder du contraste) et seuil haut,
    # la peinture reste pleine par grandes zones et s'efface franchement dans les
    # creux (jusqu'au béton nu), pour une usure bien visible mais pas totale.
    coarse = imaging.noise(SIZE, SIZE, 80, 6.0).point(
        lambda p: 255 if p > 135 else int(p * 255 / 135))
    return ImageChops.multiply(fine, coarse)


def add_marking(img, shape_draw, color, base_alpha=235):
    """Peint un marquage (défini par shape_draw) en simulant l'usure : on dessine
    sur un calque, on module son alpha par un masque d'usure, puis on compose."""
    layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    mask = Image.new("L", (SIZE, SIZE), 0)
    shape_draw(ImageDraw.Draw(mask))  # blanc là où il y a de la peinture
    paint = Image.new("RGBA", (SIZE, SIZE), color + (255,))
    alpha = ImageChops.multiply(mask, worn_alpha(base_alpha))
    layer.paste(paint, (0, 0), alpha)
    return Image.alpha_composite(img.convert("RGBA"), layer).convert("RGB")


def main(out_path):
    full = SIZE / 2.0
    cx = cy = full
    img = concrete()

    white = (228, 228, 224)

    # Cercle de poser (anneau blanc).
    def ring(d):
        r_out, r_in = full * 0.86, full * 0.80
        d.ellipse([cx - r_out, cy - r_out, cx + r_out, cy + r_out], fill=255)
        d.ellipse([cx - r_in, cy - r_in, cx + r_in, cy + r_in], fill=0)

    img = add_marking(img, ring, white, base_alpha=225)

    # Grand H blanc au centre (marquage d'hélistation civile, sans croix).
    half_w = full * 0.22
    half_l = full * 0.28
    stroke = full * 0.09

    def letter_h(d):
        d.rectangle([cx - half_w, cy - half_l, cx - half_w + stroke, cy + half_l], fill=255)
        d.rectangle([cx + half_w - stroke, cy - half_l, cx + half_w, cy + half_l], fill=255)
        d.rectangle([cx - half_w, cy - stroke / 2, cx + half_w, cy + stroke / 2], fill=255)

    img = add_marking(img, letter_h, white, base_alpha=240)

    # Salissure finale : léger assombrissement vers le bord du disque (vignette).
    vignette = Image.new("L", (SIZE, SIZE), 0)
    vd = ImageDraw.Draw(vignette)
    vd.ellipse([cx - full, cy - full, cx + full, cy + full], fill=255)
    vignette = vignette.filter(ImageFilter.GaussianBlur(SIZE * 0.05)).point(
        lambda p: 255 - int((255 - p) * 0.5))
    dark = Image.new("RGB", (SIZE, SIZE), (90, 90, 92))
    img = Image.composite(img, dark, vignette)

    img.save(out_path)
    print("[texture] écrit", out_path, img.size)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "helipad.png")
