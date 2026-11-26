#!/usr/bin/env python3
"""
interieur_4096.py
Agrandit en 4096 px l'atlas intérieur du modèle 3dexport (sa_313b_interior_c.jpg,
1024 px, blocs JPEG dès la source) pour une planche de bord nette : débruitage,
agrandissement x4, légère accentuation, puis petits cadrans et voyants
redessinés à leur place, et la peinture olive vieillie (vieillir). Le
dépliage ne change pas.

Les cadrans du manuel de vol absents de FlightGear (tachymètre double, unité
triple, pas, volts, carburant, servo, montre) sont redessinés sans aiguilles :
le moteur les anime. Ceux qui restent (anémomètre, vario, altimètre) sont
couverts par les instruments FlightGear. Le résultat dérive du modèle commercial : il reste à côté du
FBX, hors dépôt ; convertir_3dexport.py le prend s'il y est.

Usage :
    python3 tools/modele/interieur_4096.py .../civil/sa_313b_interior_c.jpg \\
        .../civil/sa_313b_interior_c_4096.jpg

Auteur : O. Booklage
Date : novembre 2026
Licence : GPL v2
"""

import colorsys
import math
import sys

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ECHELLE = 4
SURECH = 4  # suréchantillonnage du dessin, pour des traits lisses
POLICE = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"

FOND = (20, 20, 22)
TRAIT = (232, 232, 226)
VERT = (40, 170, 60)
ROUGE = (205, 35, 30)
JAUNE = (225, 180, 30)

# Vieillissement de la peinture olive (vieillir) : amplitudes en fraction de
# la luminance, nombre de rayures et d'éclats sur l'atlas 4096, teinte de
# l'apprêt qui apparaît dessous.
MARBRURE = 0.09
GRAIN = 0.035
RAYURES = 2500
ECLATS = 5000
APPRET = (128, 122, 100)

# Console sous la planche (disques de 7 faces) : centre et rayon en px de
# l'atlas 1024. Les voyants sont peints éteints, lentille sombre de leur
# couleur : le moteur pose dessus leur lumière quand ils s'allument
# (LoadedHelicopterPlanche.cpp). Les disjoncteurs, en dessous, sont des
# boutons gris clair.
VOYANTS = [
    (264.4, 702.5, 2.3), (254.8, 713.1, 2.6), (249.7, 800.8, 2.5), (252.1, 785.1, 2.4),
    (242.3, 873.0, 2.6),
]
DISJONCTEURS = [
    (669.3, 973.9, 2.1), (248.1, 815.8, 1.9), (733.5, 979.2, 1.9), (666.9, 988.3, 2.0),
    (683.3, 977.2, 1.9), (449.1, 1017.7, 2.0), (747.1, 953.0, 1.8), (267.4, 718.1, 2.0),
    (745.2, 970.2, 1.8),
]


# Cadrans dessinés, d'après le manuel de vol SE 3130 (figure 2-5a et
# graduations de l'appendice 1) : centre et rayon de la face en px de l'atlas
# 1024. La face couvre 90 % du disque plat du .glb (97 % pour la montre). Son
# centre est celui du disque, décalé vers la droite et le haut du pilote de
# la moitié du flanc du cylindre vu de l'oeil (voir CadranDessine dans
# src/render/LoadedHelicopterPlanche.cpp, mêmes décalages en mm), puis
# ramené dans l'atlas par le dépliage du disque (barycentrique, 1 px = 1,6 mm).
# Pas d'aiguilles : le moteur les pose et les anime.
CADRANS = [
    (570.42, 941.46, 22.05, "tachymetre"),
    (493.64, 945.02, 24.1, "triple"),
    (536.86, 992.16, 23.2, "pas"),
    (510.06, 685.18, 17.2, "volts"),
    (837.26, 215.18, 17.0, "carburant"),
    (878.64, 206.14, 12.4, "servo"),
    (509.17, 627.76, 18.2, "montre"),
    # Côté copilote, hors figure du manuel : bille-aiguille et thermomètre.
    (647.15, 998.79, 11.34, "virage"),
    (554.18, 678.64, 12.06, "air"),
]


def echelle(debut, etendue, vmin, vmax, pas, chiffres, arcs=(), pivot=0.0, r_ext=0.95,
            r_maj=0.78, r_min=0.86, r_chiffres=0.62, police=0.19, r_arc=0.88, l_arc=0.08):
    """Une échelle : angle de 'vmin' en degrés depuis midi (sens horaire),
    étendue, bornes, pas des graduations, valeurs chiffrées, arcs colorés.
    'pivot' place son centre sous le centre du cadran (en rayons) ; les
    rayons des traits, chiffres et arcs sont en fractions du rayon."""
    return dict(debut=debut, etendue=etendue, vmin=vmin, vmax=vmax, pas=pas,
                chiffres=chiffres, arcs=arcs, pivot=pivot, r_ext=r_ext, r_maj=r_maj,
                r_min=r_min, r_chiffres=r_chiffres, police=police, r_arc=r_arc, l_arc=l_arc)


# Mêmes échelles que src/render/PlancheBord.hpp : changer l'une, c'est changer
# l'autre. Textes : (texte, hauteur sous le centre en rayons, taille).
FACES = {
    "tachymetre": dict(
        echelles=[
            echelle(-135, 270, 0, 40000, 2000, {0: "0", 10000: "10", 20000: "20", 30000: "30",
                                                40000: "40"},
                    arcs=[(32000, 34000, VERT), (35000, 36000, ROUGE)]),
            echelle(-135, 270, 0, 420, 20, {100: "10", 200: "20", 300: "30", 400: "40"},
                    arcs=[(340, 380, VERT), (410, 420, ROUGE)], r_ext=0.50, r_maj=0.42,
                    r_min=0.46, r_chiffres=0.31, police=0.12, r_arc=0.52, l_arc=0.05),
        ],
        textes=[("ROTOR", 0.20, 0.09), ("TURBINE", 0.80, 0.10)], moyeux=[0.0]),
    "triple": dict(
        echelles=[
            echelle(-65, 130, 0, 800, 50, {0: "0", 200: "2", 400: "4", 600: "6", 800: "8"},
                    arcs=[(400, 480, VERT), (545, 565, ROUGE)]),
            echelle(-100, 85, 0, 120, 10, {0: "0", 60: "6", 120: "12"}, arcs=[(20, 85, VERT)],
                    pivot=0.45, r_ext=0.45, r_maj=0.36, r_min=0.40, r_chiffres=0.26,
                    police=0.10, r_arc=0.42, l_arc=0.05),
            echelle(100, -85, 0, 0.8, 0.1, {0: "0", 0.4: "4", 0.8: "8"},
                    arcs=[(0.0, 0.3, ROUGE), (0.5, 0.7, VERT)], pivot=0.45, r_ext=0.45,
                    r_maj=0.36, r_min=0.40, r_chiffres=0.26, police=0.10, r_arc=0.42, l_arc=0.05),
        ],
        textes=[("TUYÈRE x100", -0.40, 0.09), ("HUILE", 0.86, 0.09)], moyeux=[0.0, 0.45]),
    "pas": dict(
        echelles=[echelle(-75, 150, 0, 20, 1, {0: "0", 5: "5", 10: "10", 15: "15", 20: "20"},
                          arcs=[(14.5, 15.0, ROUGE)], l_arc=0.12)],
        textes=[("PAS", 0.36, 0.12), ("degrés", 0.54, 0.10)], moyeux=[0.0]),
    "volts": dict(
        echelles=[echelle(-60, 120, 0, 30, 2, {0: "0", 10: "10", 20: "20", 30: "30"},
                          arcs=[(26, 29, VERT)])],
        textes=[("VOLTS", 0.40, 0.13)], moyeux=[0.0]),
    "carburant": dict(
        echelles=[echelle(-60, 120, 0, 150, 10, {0: "0", 30: "3", 60: "6", 90: "9", 120: "12",
                                                 150: "15"},
                          arcs=[(0, 4, ROUGE)], police=0.17)],
        textes=[("CARBURANT", 0.38, 0.10), ("US gal x10", 0.56, 0.09)], moyeux=[0.0]),
    "servo": dict(
        echelles=[echelle(-135, 270, 0, 40, 5, {0: "0", 10: "10", 20: "20", 30: "30", 40: "40"},
                          arcs=[(30, 40, VERT)], police=0.22)],
        textes=[("SERVO", 0.74, 0.13)], moyeux=[0.0]),
    # Repères du taux standard (3 deg/s) à 25 deg ; tube de la bille en bas
    # (voir TUBE, et BILLE_* dans LoadedHelicopterPlanche.cpp).
    "virage": dict(
        echelles=[echelle(-50, 100, -6, 6, 3, {-3: "G", 3: "D"}, r_chiffres=0.58, police=0.24)],
        textes=[], moyeux=[0.0], tube=True),
    "air": dict(
        echelles=[echelle(-135, 270, -30, 50, 5, {-20: "-20", 0: "0", 20: "20", 40: "40"},
                          police=0.22)],
        textes=[("°C", 0.74, 0.18)], moyeux=[0.0]),
}

# Tube de la bille : arc de rayon TUBE_ARC centré TUBE_CENTRE au-dessus du
# centre (en rayons), sur +-TUBE_DEMI_DEG autour du bas.
TUBE_CENTRE, TUBE_ARC, TUBE_DEMI_DEG, TUBE_LARGEUR = 0.35, 0.85, 31, 0.22


def police(taille):
    try:
        return ImageFont.truetype(POLICE, max(6, int(taille)))
    except OSError:
        return ImageFont.load_default()


def point(cx, cy, r, angle):
    """Point à 'r' du centre (cx, cy), angle en degrés depuis midi, sens horaire."""
    a = math.radians(angle)
    return (cx + r * math.sin(a), cy - r * math.cos(a))


def face_montre(d, c, S, epais):
    for m in range(60):
        heure = m % 5 == 0
        d.line([point(c, c, S * (0.80 if heure else 0.88), m * 6), point(c, c, S * 0.95, m * 6)],
               fill=TRAIT, width=epais * (2 if heure else 1))
    f = police(S * 0.24)
    for h, texte in ((12, "12"), (3, "3"), (6, "6"), (9, "9")):
        d.text(point(c, c, S * 0.60, h * 30), texte, font=f, fill=TRAIT, anchor="mm")


def dessiner_echelle(d, c, S, e, epais):
    cy = c + e["pivot"] * S
    angle = lambda v: e["debut"] + e["etendue"] * (v - e["vmin"]) / (e["vmax"] - e["vmin"])
    # Arcs colorés sous les graduations (PIL compte les angles depuis 3 h).
    for v0, v1, couleur in e["arcs"]:
        r = e["r_arc"] * S
        a0, a1 = sorted((angle(v0), angle(v1)))
        d.arc([c - r, cy - r, c + r, cy + r], a0 - 90, a1 - 90, fill=couleur,
              width=max(1, int(S * e["l_arc"])))
    n = int(round((e["vmax"] - e["vmin"]) / e["pas"]))
    for i in range(n + 1):
        v = e["vmin"] + i * e["pas"]
        chiffre = any(abs(v - k) < 1e-6 for k in e["chiffres"])
        r0 = (e["r_maj"] if chiffre else e["r_min"]) * S
        d.line([point(c, cy, r0, angle(v)), point(c, cy, e["r_ext"] * S, angle(v))],
               fill=TRAIT, width=epais * (2 if chiffre else 1))
    f = police(S * e["police"])
    for v, texte in e["chiffres"].items():
        d.text(point(c, cy, e["r_chiffres"] * S, angle(v)), texte, font=f, fill=TRAIT,
               anchor="mm")


def face_cadran(R, nature):
    """Face de cadran de rayon R (px de l'atlas final), sans aiguilles."""
    S = R * SURECH
    W = int(2 * S)
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = S
    d.ellipse([0, 0, W - 1, W - 1], fill=FOND + (255,))
    epais = max(1, int(S * 0.03))
    if nature == "montre":
        face_montre(d, c, S, epais)
    else:
        spec = FACES[nature]
        for e in spec["echelles"]:
            dessiner_echelle(d, c, S, e, epais)
        if R > 40:  # sinon illisibles
            for texte, dy, taille in spec["textes"]:
                d.text((c, c + dy * S), texte, font=police(S * taille), fill=TRAIT, anchor="mm")
        if spec.get("tube"):
            cy, r = c - TUBE_CENTRE * S, TUBE_ARC * S
            d.arc([c - r, cy - r, c + r, cy + r], 90 - TUBE_DEMI_DEG, 90 + TUBE_DEMI_DEG,
                  fill=(200, 200, 194), width=int(TUBE_LARGEUR * S))
            # Deux traits encadrent la bille au centre.
            for a in (-7, 7):
                d.line([point(c, cy, r - 0.12 * S, 180 + a), point(c, cy, r + 0.12 * S, 180 + a)],
                       fill=FOND, width=epais)
        # Moyeux sous les aiguilles du moteur.
        for p in spec["moyeux"]:
            r = 0.07 * S
            d.ellipse([c - r, c + p * S - r, c + r, c + p * S + r], fill=(70, 70, 70))
    return img.resize((int(2 * R), int(2 * R)), Image.LANCZOS)


def lentille(R, couleur):
    """Dôme de voyant de rayon R : dégradé radial, bord sombre, reflet."""
    S = R * SURECH
    W = int(2 * S)
    y, x = np.mgrid[0:W, 0:W].astype(np.float32) + 0.5
    t = np.sqrt((x - S) ** 2 + (y - S) ** 2) / S
    base = np.array(couleur, np.float32)
    rgb = base[None, None, :] * np.clip(1.15 - 0.55 * t, 0.0, 1.2)[..., None]
    reflet = np.exp(-(((x - 0.65 * S) ** 2 + (y - 0.65 * S) ** 2) / (0.09 * S * S)))
    rgb = rgb + 200.0 * reflet[..., None]
    alpha = np.clip((1.0 - t) * S * 0.5, 0.0, 1.0) * 255.0
    img = np.dstack([np.clip(rgb, 0, 255), alpha]).astype(np.uint8)
    return Image.fromarray(img, "RGBA").resize((int(2 * R), int(2 * R)), Image.LANCZOS)


def couleur_lentille(src, u, v, r, clarte):
    """Couleur d'origine du voyant, saturée, à la clarté voulue ; gris clair
    s'il n'a pas de teinte."""
    y0, y1, x0, x1 = int(v - r), int(v + r) + 1, int(u - r), int(u + r) + 1
    moy = src[max(0, y0):y1, max(0, x0):x1].reshape(-1, 3).mean(0) / 255.0
    h, s, _ = colorsys.rgb_to_hsv(*moy)
    if s < 0.25:
        return (190, 190, 185)
    r_, g_, b_ = colorsys.hsv_to_rgb(h, max(s, 0.8), clarte)
    return (int(r_ * 255), int(g_ * 255), int(b_ * 255))


def vieillir(rgb, graine=313):
    """Peinture vieillie sur les surfaces olive : marbrures, grain, rayures
    fines et petits éclats près des arêtes. 'rgb' est l'atlas agrandi (uint8)."""
    alea = np.random.default_rng(graine)
    h, w = rgb.shape[:2]
    hsv = cv2.cvtColor(rgb, cv2.COLOR_RGB2HSV)
    # Peinture olive : teinte jaune-vert, un peu saturée, ni noire ni claire.
    olive = ((hsv[..., 0] > 18) & (hsv[..., 0] < 60) & (hsv[..., 1] > 40) &
             (hsv[..., 2] > 30) & (hsv[..., 2] < 190)).astype(np.float32)
    olive = cv2.GaussianBlur(olive, (0, 0), 3.0)
    img = rgb.astype(np.float32)

    def bruit(sigma):
        n = cv2.GaussianBlur(alea.standard_normal((h, w)).astype(np.float32), (0, 0), sigma)
        return n / (n.std() + 1e-6)

    # Marbrures (taches larges) et grain fin, en facteur de luminance.
    facteur = 1.0 + olive * (MARBRURE * bruit(40.0) + GRAIN * bruit(1.0))
    img *= facteur[..., None]

    # Rayures et éclats : peinture partie, apprêt clair dessous. L'usure se
    # concentre près des arêtes, là où les mains et les bottes frottent.
    calque = np.zeros((h, w), np.float32)
    aretes = cv2.Canny(cv2.cvtColor(rgb, cv2.COLOR_RGB2GRAY), 40, 120)
    aretes = cv2.dilate(aretes, np.ones((5, 5), np.uint8)) > 0
    ys, xs = np.nonzero(aretes)
    for i in alea.choice(len(xs), size=min(RAYURES, len(xs)), replace=False):
        x = xs[i] + alea.normal(0, 12)
        y = ys[i] + alea.normal(0, 12)
        a, lg = alea.uniform(0, math.pi), alea.uniform(4, 18)
        cv2.line(calque, (int(x), int(y)), (int(x + lg * math.cos(a)), int(y + lg * math.sin(a))),
                 alea.uniform(0.2, 0.5), 1, cv2.LINE_AA)
    for i in alea.choice(len(xs), size=min(ECLATS, len(xs)), replace=False):
        cv2.circle(calque, (int(xs[i]), int(ys[i])), int(alea.integers(1, 3)), 0.55, -1, cv2.LINE_AA)
    calque = np.clip(calque, 0, 1) * olive
    img = img * (1 - calque[..., None]) + np.array(APPRET, np.float32) * calque[..., None]
    return np.clip(img, 0, 255).astype(np.uint8)


def coller(img, motif, u, v):
    """Colle 'motif' centré sur le point (u, v) de l'atlas 1024."""
    img.alpha_composite(motif, (int(round(u * ECHELLE - motif.width / 2)),
                                int(round(v * ECHELLE - motif.height / 2))))


def main():
    entree, sortie = sys.argv[1:3]
    bgr = cv2.imread(entree)
    if bgr is None:
        sys.exit("interieur_4096 : lecture impossible de %s" % entree)
    # Débruitage non local (efface les blocs JPEG), x4, légère accentuation.
    propre = cv2.fastNlMeansDenoisingColored(bgr, None, 4, 4, 5, 15)
    grand = cv2.resize(propre, None, fx=ECHELLE, fy=ECHELLE, interpolation=cv2.INTER_LANCZOS4)
    flou = cv2.GaussianBlur(grand, (0, 0), 2.0)
    net = cv2.addWeighted(grand, 1.5, flou, -0.5, 0)
    img = Image.fromarray(vieillir(cv2.cvtColor(net, cv2.COLOR_BGR2RGB))).convert("RGBA")

    for u, v, r, nature in CADRANS:
        coller(img, face_cadran(r * ECHELLE, nature), u, v)
    src = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
    for u, v, r in VOYANTS:
        coller(img, lentille(r * ECHELLE, couleur_lentille(src, u, v, r, 0.30)), u, v)
    for u, v, r in DISJONCTEURS:
        coller(img, lentille(r * ECHELLE, (175, 175, 170)), u, v)

    img.convert("RGB").save(sortie, quality=92, subsampling=0)
    print("interieur_4096 : écrit", sortie, img.size)


if __name__ == "__main__":
    main()
