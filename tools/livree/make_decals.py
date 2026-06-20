#!/usr/bin/env python3
# Genere les decalques (textes blancs sur fond transparent) appliques en 3D sur
# les flancs de la livree Gendarmerie : le mot "GENDARMERIE" et l'immatriculation
# "F-BRHP". Chaque decalque est un PNG RGBA pose ensuite sur un quad texture.
import os
from PIL import Image, ImageDraw, ImageFont

OUT = "assets/models/Alouette-II/Models"
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def make_text(text, w, h, font_px, dst, tracking=0):
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(FONT, font_px)
    # Largeur totale en tenant compte d'un eventuel espacement entre lettres.
    widths = [d.textlength(c, font=font) for c in text]
    total = sum(widths) + tracking * (len(text) - 1)
    x = (w - total) / 2
    # Centrage vertical d'apres la boite du texte.
    bbox = font.getbbox(text)
    y = (h - (bbox[3] - bbox[1])) / 2 - bbox[1]
    for c, cw in zip(text, widths):
        d.text((x, y), c, font=font, fill=(255, 255, 255, 255))
        x += cw + tracking
    img.save(os.path.join(OUT, dst))
    print("decalque ecrit ->", dst, img.size)


# "GENDARMERIE" : lettres serrees et larges, comme sur la cellule reelle.
make_text("GENDARMERIE", 1024, 160, 130, "decal-gendarmerie.png", tracking=6)
# Immatriculation de l'appareil de "Peau d'Ane".
make_text("F-BRHP", 640, 200, 150, "decal-fbrhp.png", tracking=4)

# Liseré blanc plein : un petit rectangle opaque, etire ensuite en fine bande.
Image.new("RGBA", (16, 16), (255, 255, 255, 255)).save(os.path.join(OUT, "decal-stripe.png"))
print("decalque ecrit -> decal-stripe.png (16, 16)")
