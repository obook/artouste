#!/usr/bin/env python3
"""
Nom du fichier : formes_base.py
Description : Plans, textures et volumes du décor peint de la base de Pau :
              hangars, bâtiments, voitures, appareils.
Auteur : O. Booklage
Date : Août 2026
Licence : GPL v2
"""

import math

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

AXE = 125.0                       # cap de la piste 13/31, mesuré sur l'ortho
# Soleil de midi en été, hauteur ~65 degrés : une hauteur h projette 0,47 h,
# vers le nord (l'ombre part vers le haut de l'image).
OMBRE_FACTEUR = 0.47
OMBRE_AZIMUT = 0.0

BETON      = (134, 134, 130)
BETON_CLAIR= (150, 150, 146)
BETON_USE  = (120, 120, 118)
JOINT      = (139, 139, 137)
BITUME     = ( 88,  88,  86)
BITUME_USE = (104, 104, 101)
TOIT_TOLE  = (176, 178, 178)
TOIT_TOLE2 = (163, 165, 166)
TOIT_MAT   = (122, 112, 104)
TOIT_PLAT  = (138, 136, 130)
MARQUE     = (222, 220, 210)
MARQUE_J   = (206, 186,  92)
HERBE      = (104, 118,  92)
ARBRE      = ( 62,  80,  66)
OMBRE      = (  0,   0,   0,  62)
VEHICULE   = [(180,180,182),(70,72,76),(140,30,30),(40,60,110),(210,210,205)]
HELICO     = ( 74,  80,  66)

class Plan:
    """Dessin en mètres dans le repère de la piste. u court le long de l'axe
    (positif vers le sud-est), v en travers (positif vers le sud-ouest)."""

    def __init__(self, taille_px, centre_px, mpp):
        self.mpp = mpp
        self.img = Image.new("RGB", taille_px, HERBE)
        self.ombres = Image.new("RGBA", taille_px, (0, 0, 0, 0))
        self.cx, self.cy = centre_px
        a = math.radians(AXE)
        self.ux, self.uy = math.sin(a), -math.cos(a)
        self.vx, self.vy = math.cos(a), math.sin(a)
        self.dr = ImageDraw.Draw(self.img)
        self.dro = ImageDraw.Draw(self.ombres)

    @property
    def fin(self):
        """Vrai quand l'échelle permet le détail (marquages, véhicules)."""
        return self.mpp <= 0.6

    def px(self, u, v):
        return (self.cx + (u * self.ux + v * self.vx) / self.mpp,
                self.cy + (u * self.uy + v * self.vy) / self.mpp)

    def coins(self, u0, v0, lu, lv):
        return [self.px(u0, v0), self.px(u0 + lu, v0),
                self.px(u0 + lu, v0 + lv), self.px(u0, v0 + lv)]

    def dalle(self, u0, v0, lu, lv, couleur, texture=None):
        c = self.coins(u0, v0, lu, lv)
        if texture is None:
            self.dr.polygon(c, fill=couleur)
        else:
            m = Image.new("L", self.img.size, 0)
            ImageDraw.Draw(m).polygon(c, fill=255)
            self.img.paste(texture, (0, 0), m)
            self.dr = ImageDraw.Draw(self.img)

    def volume(self, u0, v0, lu, lv, hauteur, couleur):
        """Bâtiment : son ombre portée d'abord, puis son toit."""
        d = hauteur * OMBRE_FACTEUR
        du = d * math.cos(math.radians(OMBRE_AZIMUT - AXE))
        dv = d * math.sin(math.radians(OMBRE_AZIMUT - AXE))
        self.dro.polygon(self.coins(u0 + du, v0 + dv, lu, lv), fill=OMBRE)
        self.dr.polygon(self.coins(u0, v0, lu, lv), fill=couleur)

    def ligne(self, u0, v0, u1, v1, largeur_m, couleur):
        w = max(1, int(round(largeur_m / self.mpp)))
        self.dr.line([self.px(u0, v0), self.px(u1, v1)], fill=couleur, width=w)

    def disque(self, u, v, rayon_m, couleur, contour=None, epaisseur_m=0.0):
        x, y = self.px(u, v); r = rayon_m / self.mpp
        self.dr.ellipse([x - r, y - r, x + r, y + r], fill=couleur, outline=contour,
                        width=max(1, int(round(epaisseur_m / self.mpp))) if epaisseur_m else 1)


def texture(sources, taille, graine=0):
    """Pave une surface avec un morceau de photo réelle, en MIROIR.

    Le miroir n'est pas un ornement : une tuile retournée présente à son voisin
    le bord que celui-ci lui présente, si bien que le raccord est continu. Un
    pavage à retournements tirés au hasard, lui, laisse une grille de coutures
    visibles, défaut constaté au premier essai."""
    if not isinstance(sources, (list, tuple)):
        sources = [sources]
    m = sources[0].size[0]
    out = Image.new("RGB", taille)
    for jy, y in enumerate(range(0, taille[1], m)):
        for jx, x in enumerate(range(0, taille[0], m)):
            # trois herbes différentes en damier : le seul miroir donnait une
            # symétrie en kaléidoscope, visible dès qu'on la cherche
            t = sources[(jx + 2 * jy) % len(sources)]
            if jx % 2:
                t = t.transpose(Image.FLIP_LEFT_RIGHT)
            if jy % 2:
                t = t.transpose(Image.FLIP_TOP_BOTTOM)
            out.paste(t, (x, y))
    return out


def surface_lisse(couleur, taille, grain, echelle_m, mpp, graine):
    """Fabrique une surface minérale (béton, bitume) plutôt que de la prélever :
    les rares aires nettes de la plateforme portent toutes des bâtiments ou des
    marquages, qui se retrouveraient répétés dans le décor."""
    rng = np.random.default_rng(graine)
    h, w = taille[1], taille[0]
    petit = (max(4, int(w * mpp / echelle_m)), max(4, int(h * mpp / echelle_m)))
    tache = ndimage.gaussian_filter(rng.normal(0, 1, (petit[1], petit[0])).astype(np.float32), 1.0)
    tache = np.asarray(Image.fromarray(tache, mode="F").resize((w, h), Image.BICUBIC))
    fin = ndimage.gaussian_filter(rng.normal(0, 1, (h, w)).astype(np.float32), 0.7)
    v = tache / max(tache.std(), 1e-6) * grain + fin / max(fin.std(), 1e-6) * (grain * 0.45)
    a = np.clip(np.asarray(couleur, dtype=np.float32)[None, None, :] + v[..., None], 0, 255)
    return Image.fromarray(a.astype(np.uint8))


def hangar(p, u, v, lu, lv, hauteur=12.0):
    """Hangar à nef : toiture en tôle nervurée, faîtage, portes sur pignon."""
    p.volume(u, v, lu, lv, hauteur, TOIT_TOLE)
    if not p.fin:
        return
    for k in range(int(lv / 2.4)):                       # nervures de la tôle
        vv = v + 1.2 + k * 2.4
        p.ligne(u, vv, u + lu, vv, 0.5, TOIT_TOLE2)
    p.ligne(u, v + lv / 2, u + lu, v + lv / 2, 1.6, (196, 198, 198))   # faîtage
    for k in range(4):                                   # lanterneaux
        uu = u + lu * (0.2 + 0.2 * k)
        p.dalle(uu, v + lv / 2 - 3.0, 8.0, 6.0, (208, 210, 206))
    for k in range(int(lu / 9)):                         # salissures de toiture
        p.dalle(u + k * 9 + 2, v + 2, 5.0, lv - 4, (172, 174, 174))
    p.dalle(u, v + lv, lu, 1.2, (96, 96, 94))            # rive sombre
    p.dalle(u - 12, v, 12, lv, BETON_CLAIR)              # seuil bétonné


def batiment(p, u, v, lu, lv, hauteur, toit):
    p.volume(u, v, lu, lv, hauteur, toit)
    if not p.fin:
        return
    if toit is TOIT_PLAT:
        rng = np.random.default_rng(int(abs(u) * 7 + abs(v)))
        for _ in range(int(lu * lv / 260)):              # blocs de ventilation
            uu = u + rng.uniform(2, max(3, lu - 4)); vv = v + rng.uniform(2, max(3, lv - 4))
            p.dalle(uu, vv, rng.uniform(1.5, 3.5), rng.uniform(1.5, 3.0), (112, 110, 106))
    else:
        p.ligne(u, v + lv / 2, u + lu, v + lv / 2, 0.8, (150, 138, 128))


def voiture(p, u, v, cap_u=True, graine=0):
    rng = np.random.default_rng(graine)
    c = VEHICULE[int(rng.integers(0, len(VEHICULE)))]
    lu, lv = (4.4, 1.9) if cap_u else (1.9, 4.4)
    p.volume(u, v, lu, lv, 1.6, c)


def appareil(p, u, v, graine=0):
    """Silhouette d'hélicoptère au parking : fuselage, poutre, rotor."""
    rng = np.random.default_rng(graine)
    a = rng.uniform(-12, 12)                              # petit désalignement
    ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
    def loc(du, dv):
        return (u + du * ca - dv * sa, v + du * sa + dv * ca)
    p.volume(*loc(-4.0, -1.3), 8.0, 2.6, 3.0, HELICO)     # cabine
    u2, v2 = loc(4.0, -0.5)
    p.volume(u2, v2, 6.5, 1.0, 2.6, HELICO)               # poutre de queue
    for k in range(4):                                    # pales
        ang = a + k * 45.0
        du = 7.2 * math.cos(math.radians(ang)); dv = 7.2 * math.sin(math.radians(ang))
        p.ligne(u, v, u + du, v + dv, 0.45, (58, 60, 58))
