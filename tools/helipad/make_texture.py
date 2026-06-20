#!/usr/bin/env python3
"""
make_texture.py
Genere la texture de l'helistation hospitaliere : beton use et sali, cercle de
poser, croix blanche et grand H rouge (marquage OACI d'un heliport d'hopital).
L'image est carree ; le disque de l'helipad y est inscrit (les coins, hors
disque, ne se voient pas).

A lancer avec le Python du venv du projet (voir tools/.venv) :
    tools/.venv/bin/python tools/helipad/make_texture.py assets/models/helipad/helipad.png

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

SIZE = 2048  # cote de l'image, en pixels


def noise(sigma, blur=0.0):
    img = Image.effect_noise((SIZE, SIZE), sigma).convert("L")
    if blur > 0.0:
        img = img.filter(ImageFilter.GaussianBlur(blur))
    return img


def concrete():
    """Dalle de beton : gris moyen, grain a deux echelles, taches sombres et
    quelques fissures, plus un assombrissement vers le bord (salissure)."""
    base = Image.new("RGB", (SIZE, SIZE), (146, 147, 150))
    fine = noise(34, 1.0)
    coarse = noise(60, 9.0)
    grain = ImageChops.multiply(fine, coarse).point(lambda p: 110 + p // 3)
    grain_rgb = Image.merge("RGB", (grain, grain, grain))
    img = Image.blend(base, grain_rgb, 0.45)

    draw = ImageDraw.Draw(img, "RGBA")

    # Taches d'huile / d'usure : ellipses sombres floutees, disposees au pseudo
    # hasard (positions fixes pour un resultat reproductible).
    stains = [(0.30, 0.32, 0.10), (0.66, 0.28, 0.07), (0.58, 0.70, 0.12),
              (0.38, 0.66, 0.06), (0.72, 0.55, 0.05), (0.24, 0.52, 0.05)]
    stain_layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    sd = ImageDraw.Draw(stain_layer, "RGBA")
    for fx, fy, fr in stains:
        cx, cy, r = fx * SIZE, fy * SIZE, fr * SIZE
        sd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(40, 38, 36, 110))
    stain_layer = stain_layer.filter(ImageFilter.GaussianBlur(SIZE * 0.02))
    img = Image.alpha_composite(img.convert("RGBA"), stain_layer).convert("RGB")

    draw = ImageDraw.Draw(img, "RGBA")
    # Fissures : courtes lignes sombres irregulieres.
    cracks = [(0.20, 0.40, 0.46, 0.30), (0.55, 0.20, 0.62, 0.48),
              (0.50, 0.80, 0.40, 0.60), (0.78, 0.62, 0.66, 0.74)]
    for x0, y0, x1, y1 in cracks:
        pts = []
        for t in range(7):
            f = t / 6.0
            x = (x0 + (x1 - x0) * f + 0.012 * math.sin(t * 2.1)) * SIZE
            y = (y0 + (y1 - y0) * f + 0.012 * math.cos(t * 1.7)) * SIZE
            pts.append((x, y))
        draw.line(pts, fill=(60, 60, 62, 150), width=3)
    return img


def worn_alpha(base_alpha):
    """Masque d'alpha pour une peinture usee : surtout opaque, avec des plages
    plus pales (la peinture s'efface par endroits)."""
    wear = noise(70, 2.0).point(lambda p: base_alpha if p > 64 else base_alpha - 70 + p)
    return wear


def add_marking(img, shape_draw, color, base_alpha=235):
    """Peint un marquage (defini par shape_draw) en simulant l'usure : on dessine
    sur un calque, on module son alpha par un masque d'usure, puis on compose."""
    layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    mask = Image.new("L", (SIZE, SIZE), 0)
    shape_draw(ImageDraw.Draw(mask))  # blanc la ou il y a de la peinture
    paint = Image.new("RGBA", (SIZE, SIZE), color + (255,))
    alpha = ImageChops.multiply(mask, worn_alpha(base_alpha))
    layer.paste(paint, (0, 0), alpha)
    return Image.alpha_composite(img.convert("RGBA"), layer).convert("RGB")


def add_skid_marks(img):
    """Deux trainees sombres paralleles laissees par les patins, le long de l'axe
    avant-arriere (axe X de la texture). Posees par-dessus le marquage, comme un
    frottement de caoutchouc accumule au fil des poses."""
    full = SIZE / 2.0
    cx = cy = full
    length = full * 0.22   # demi-longueur de la trace
    width = full * 0.045   # demi-epaisseur
    offset = full * 0.14   # ecart lateral entre les deux patins

    layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer, "RGBA")
    for sign in (-1, 1):
        oy = cy + sign * offset
        d.rounded_rectangle([cx - length, oy - width, cx + length, oy + width],
                            radius=width, fill=(34, 32, 30, 130))
        # Quelques stries plus sombres pour le grain du frottement.
        for k in range(-2, 3):
            yy = oy + k * (width * 0.4)
            d.line([(cx - length * 0.95, yy), (cx + length * 0.95, yy)],
                   fill=(20, 19, 18, 90), width=2)
    layer = layer.filter(ImageFilter.GaussianBlur(SIZE * 0.006))
    return Image.alpha_composite(img.convert("RGBA"), layer).convert("RGB")


def main(out_path):
    full = SIZE / 2.0
    cx = cy = full
    img = concrete()

    white = (228, 228, 224)
    red = (172, 32, 30)

    # Cercle de poser (anneau blanc).
    def ring(d):
        r_out, r_in = full * 0.86, full * 0.80
        d.ellipse([cx - r_out, cy - r_out, cx + r_out, cy + r_out], fill=255)
        d.ellipse([cx - r_in, cy - r_in, cx + r_in, cy + r_in], fill=0)

    img = add_marking(img, ring, white, base_alpha=225)

    # Croix blanche (croix grecque : deux barres egales qui se croisent).
    arm = full * 0.42   # demi-longueur d'une barre
    half = full * 0.21  # demi-epaisseur d'une barre

    def cross(d):
        d.rectangle([cx - half, cy - arm, cx + half, cy + arm], fill=255)
        d.rectangle([cx - arm, cy - half, cx + arm, cy + half], fill=255)

    img = add_marking(img, cross, white, base_alpha=235)

    # Grand H rouge au centre de la croix.
    half_w = full * 0.17
    half_l = full * 0.23
    stroke = full * 0.075

    def letter_h(d):
        d.rectangle([cx - half_w, cy - half_l, cx - half_w + stroke, cy + half_l], fill=255)
        d.rectangle([cx + half_w - stroke, cy - half_l, cx + half_w, cy + half_l], fill=255)
        d.rectangle([cx - half_w, cy - stroke / 2, cx + half_w, cy + stroke / 2], fill=255)

    img = add_marking(img, letter_h, red, base_alpha=240)

    # Salissure finale : leger assombrissement vers le bord du disque (vignette).
    vignette = Image.new("L", (SIZE, SIZE), 0)
    vd = ImageDraw.Draw(vignette)
    vd.ellipse([cx - full, cy - full, cx + full, cy + full], fill=255)
    vignette = vignette.filter(ImageFilter.GaussianBlur(SIZE * 0.05)).point(
        lambda p: 255 - int((255 - p) * 0.5))
    dark = Image.new("RGB", (SIZE, SIZE), (90, 90, 92))
    img = Image.composite(img, dark, vignette)

    # Traces de patins, par-dessus tout le reste.
    img = add_skid_marks(img)

    img.save(out_path)
    print("[texture] ecrit", out_path, img.size)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "helipad.png")
