#!/usr/bin/env python3
"""
decor_base_hr.py
Décor IMAGINAIRE de la base d'hélicoptères de Pau-Uzein, dessiné à n'importe
quelle finesse (0,25 m par pixel pour les tuiles de détail, 1,80 m pour
l'orthophoto d'ensemble).

Rien ici ne prétend représenter les installations réelles, dont l'imagerie
officielle est floutée à la source : c'est un décor plausible pour un
simulateur, bâti sur le vocabulaire d'une base d'hélicoptères ordinaire.

Tout est dessiné en MÈTRES dans le repère de la piste (axe 125 degrés), jamais
en pixels : le même plan sert donc aux deux finesses, et le détail fin
(marquages, nervures de toiture, véhicules, appareils au parking) n'apparaît
que lorsque l'échelle le permet. Agrandir un dessin fait pour 1,80 m ne
donnerait que du flou.

Usage : decor_base_hr.py <sortie.png> <largeur_px> <hauteur_px> <mpp>
                          <lon_coin_no> <lat_coin_no>
"""
import math
import sys

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


def dessiner(taille_px, centre_px, mpp, textures):
    p = Plan(taille_px, centre_px, mpp)
    herbe, beton, bitume = textures
    p.img.paste(herbe, (0, 0)); p.dr = ImageDraw.Draw(p.img)

    # --- aire de stationnement le long de la piste et sa voie de circulation
    p.dalle(-780, -560, 1500, 300, BETON, beton)
    p.dalle(-780, -290, 1500, 40, BITUME, bitume)
    if p.fin:
        for k in range(int(1500 / 25)):                   # joints de dalle
            p.ligne(-780 + k * 25, -560, -780 + k * 25, -260, 0.25, JOINT)
        for k in range(int(300 / 25)):
            p.ligne(-780, -560 + k * 25, 720, -560 + k * 25, 0.25, JOINT)
        p.ligne(-780, -270, 720, -270, 0.6, MARQUE_J)     # axe de la voie

    # --- plots de poser, numérotés, avec quelques appareils garés
    rng = np.random.default_rng(5)
    for k in range(14):
        u = -720 + k * 105
        for v in (-470, -370):
            p.disque(u, v, 16, BETON_USE)
            if p.fin:
                p.disque(u, v, 15, None, contour=MARQUE, epaisseur_m=0.6)
                p.ligne(u - 6, v, u + 6, v, 0.5, MARQUE)
            if rng.random() < 0.45:
                appareil(p, u, v, graine=k * 7 + (0 if v < -400 else 1))

    # --- hangars et leur desserte
    for k in range(6):
        hangar(p, -700 + k * 190, -200, 130, 70)
    p.dalle(-780, -110, 1500, 26, BITUME, bitume)
    if p.fin:
        p.ligne(-780, -97, 720, -97, 0.5, MARQUE)

    # --- quartier technique : bâtiments, rues, stationnements
    rng = np.random.default_rng(7)
    for rangee in range(4):
        for k in range(7):
            u = -640 + k * 165 + rng.uniform(-15, 15)
            v = 40 + rangee * 130 + rng.uniform(-10, 10)
            lu, lv = rng.uniform(55, 110), rng.uniform(28, 48)
            toit = (TOIT_MAT, TOIT_PLAT, TOIT_TOLE2)[(k + rangee) % 3]
            batiment(p, u, v, lu, lv, rng.uniform(7, 13), toit)
    for rangee in range(5):
        p.dalle(-700, 10 + rangee * 130, 1350, 14, BITUME, bitume)
        if p.fin:
            for k in range(int(1350 / 12)):               # ligne axiale discontinue
                p.ligne(-700 + k * 12, 17, -694 + k * 12, 17, 0.15, MARQUE)
    for k in range(6):
        p.dalle(-620 + k * 230, 10, 12, 540, BITUME, bitume)

    # --- parkings véhicules, remplis
    for k in range(3):
        u0 = -660 + k * 150
        p.dalle(u0, 570, 120, 60, BITUME, bitume)
        if p.fin:
            for i in range(int(120 / 5.2)):
                p.ligne(u0 + 2 + i * 5.2, 572, u0 + 2 + i * 5.2, 598, 0.15, MARQUE)
                if rng.random() < 0.6:
                    voiture(p, u0 + 3 + i * 5.2, 574, cap_u=False, graine=k * 40 + i)

    # --- route de ceinture et rideau d'arbres
    p.dalle(-820, -600, 20, 1260, BITUME, bitume)
    p.dalle(760, -600, 20, 1260, BITUME, bitume)
    rng = np.random.default_rng(21)
    for v in np.arange(-560, 640, 26):
        for u in (-860, 800):
            uu = u + rng.uniform(-12, 12); vv = v + rng.uniform(-10, 10)
            r = rng.uniform(5, 10)
            d = 9.0 * OMBRE_FACTEUR
            p.dro.ellipse([p.px(uu + d, vv)[0] - r / mpp, p.px(uu + d, vv)[1] - r / mpp,
                           p.px(uu + d, vv)[0] + r / mpp, p.px(uu + d, vv)[1] + r / mpp], fill=OMBRE)
            p.disque(uu, vv, r, ARBRE)

    # les ombres, adoucies, se posent sur l'ensemble
    o = np.asarray(p.ombres).astype(np.float32)
    o[..., 3] = ndimage.gaussian_filter(o[..., 3], max(0.6, 0.9 / mpp))
    p.img = Image.alpha_composite(p.img.convert("RGBA"),
                                  Image.fromarray(o.astype(np.uint8))).convert("RGB")
    return p.img


def main():
    sortie, W, H, mpp, lonNO, latNO = (sys.argv[1], int(sys.argv[2]), int(sys.argv[3]),
                                       float(sys.argv[4]), float(sys.argv[5]), float(sys.argv[6]))
    Image.MAX_IMAGE_PIXELS = None
    ortho = Image.open("assets/terrain/pau/ortho-ign-original.jpg").convert("RGB")

    # Centre du décor : le centroïde du contour tracé à la main, en lon/lat.
    LON0, DLON = -0.4519550855, 0.0000222322
    LAT0, DLAT = 43.3943211600, 0.0000161700
    m = np.asarray(Image.open(
        "/tmp/claude-1000/-home-obooklage-Documents-GitHub-artouste/"
        "3d92b580-04b2-41f4-b7a6-790b60b02ab5/scratchpad/pau-trace/"
        "masque-zone-militaire.png").convert("L")) > 128
    ys, xs = np.nonzero(m)
    lonC, latC = LON0 + xs.mean() * DLON, LAT0 - ys.mean() * DLAT
    mx = 111320 * math.cos(math.radians(latC))
    centre = ((lonC - lonNO) * mx / mpp, (latNO - latC) * 111320 / mpp)

    # Textures : de VRAIS morceaux d'orthophoto, pris à 25 cm sur les parties
    # nettes de la même plateforme (herbe au nord de la piste, béton de l'aire
    # civile, bitume d'une voie de circulation), puis ramenés à la finesse
    # demandée. Agrandir la photo d'ensemble à 1,80 m donnait des taches floues
    # aux coutures visibles : à 25 cm il faut de la donnée à 25 cm.
    def morceau(nom, cote_m=128.0, reference=None):
        """Un morceau de photo réelle, ramené à la finesse voulue. Les herbes
        prélevées à des endroits différents n'ont pas le même ton : on les
        recale sur la première, sans quoi le pavage dessine un damier."""
        im = Image.open("/tmp/tex_%s.png" % nom).convert("RGB")   # 512 px = 128 m
        n = max(8, int(round(cote_m / mpp)))
        im = im.resize((n, n), Image.LANCZOS)
        if reference is not None:
            a = np.asarray(im, dtype=np.float32)
            b = np.asarray(reference, dtype=np.float32)
            for c in range(3):
                a[..., c] += b[..., c].mean() - a[..., c].mean()
            im = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
        return im

    herbe0 = morceau("herbe")
    textures = (texture([herbe0, morceau("herbe2", reference=herbe0),
                         morceau("herbe3", reference=herbe0)], (W, H)),
                surface_lisse(BETON, (W, H), 4.0, 3.0, mpp, 11),
                surface_lisse(BITUME, (W, H), 3.5, 2.5, mpp, 12))
    dessiner((W, H), centre, mpp, textures).save(sortie)
    print("décor écrit : %s (%d x %d, %.2f m/px)" % (sortie, W, H, mpp))


if __name__ == "__main__":
    main()
