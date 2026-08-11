#!/usr/bin/env python3
"""
make_fond.py
Fabrique le fond détaillé 2048x2048 des livrées du fuselage, à partir de
l'atlas d'origine (512, jamais modifié) et de la carte de relief cuite par
cuire_normale.py. L'agrandissement seul ne ferait que des aplats plus grands ;
le détail vient du relief : la divergence des normales tangentes redonne le
dessin exact des tôles sur l'atlas, creux assombris (poussière logée dans les
rainures) et arêtes éclaircies (peinture usée sur les angles). Un grain fin et
reproductible casse les aplats restants.

Les quatre make_* du fuselage prennent ce fond comme source : le repeint de
retint.py n'agit que sur la saturation et la luminance, le détail survit donc
au changement de couleur, et l'occlusion s'applique par-dessus comme avant.

Avec un quatrième argument (l'OBJ du seul nœud "moteur", turbine et tuyère),
la zone UV du moteur est rendue à son métal nu : gris chaud en pleine lumière,
bleui dans les creux, comme un échappement qui a chauffé. La teinte est juste
assez saturée pour échapper au masque des gris de retint.py : la turbine garde
son métal dans les quatre livrées, au lieu d'être peinte couleur carrosserie.
L'OBJ s'exporte en écartant tous les autres nœuds :

    ./build/bin/model_probe assets/models/Alouette-II/Models/alouette.ac \\
        /tmp/moteur.obj "structure,fuselage,verriere,vitre,porte,patin,barre,\\
flotteur,roue,aileron,cache,lampe,reservoir,support,tourvitre,hdr,blur,disc"

Usage : python3 tools/livree/make_fond.py [src] [relief] [dst] [moteur.obj]
Sortie par défaut : assets/models/Alouette-II/Models/texture-fond.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

SRC_DEFAUT = "assets/models/Alouette-II/Models/texture.png"
RELIEF_DEFAUT = "assets/models/Alouette-II/Models/texture-relief.png"
DST_DEFAUT = "assets/models/Alouette-II/Models/texture-fond.png"

ARETE = 1.3    # gain de luminance au sommet d'une arête convexe
CREUX = 1.6    # perte de luminance au fond d'une rainure
COURBURE_MAX = 0.15  # écrêtage de la divergence : au-delà, l'effet n'augmente plus
GRAIN = 0.025  # amplitude du grain, en fraction de la luminance
GRAINE = 313   # graine du grain : SA 313, et surtout reproductible

# Métal du moteur : acier patiné, chaud en pleine lumière, bleui dans l'ombre.
# Les deux teintes gardent une saturation d'au moins 0.19 après le gain borné,
# pour rester au-dessus du seuil de 0.18 sous lequel retint.py repeint les gris.
# Le plafond de gain est serré à dessein : les texels d'origine de la zone sont
# presque blancs, et le shader ajoute lumière et reflet par-dessus ; un plafond
# généreux rend la tuyère crème en plein soleil.
METAL_PAILLE = np.array([0.42, 0.39, 0.335])
METAL_BLEUI = np.array([0.28, 0.30, 0.35])
METAL_GAIN = (0.35, 1.05)


def courbure(chemin, taille):
    """Divergence des normales tangentes de la carte de relief : positive sur
       les arêtes convexes, négative dans les creux, nulle sur les surfaces
       planes ou en pente régulière (les tubes ronds ne marquent que leurs
       bords). Le canal vert est retourné : v monte, l'axe image descend."""
    relief = Image.open(chemin).convert("RGB").resize(taille, Image.BILINEAR)
    n = np.asarray(relief, dtype=np.float64)
    nx = (n[..., 0] - 128.0) / 127.0
    ny = (n[..., 1] - 128.0) / 127.0
    div = np.gradient(nx, axis=1) + np.gradient(-ny, axis=0)
    return np.clip(div, -COURBURE_MAX, COURBURE_MAX)


def masque_uv(chemin_obj, taille):
    """Rastérise les triangles UV de l'OBJ en un masque adouci (0..1) : les
       bords fondent sur un pixel pour ne pas cerner la zone d'un liséré."""
    uvs, faces = [], []
    for ligne in open(chemin_obj):
        p = ligne.split()
        if not p:
            continue
        if p[0] == "vt":
            uvs.append((float(p[1]), float(p[2])))
        elif p[0] == "f":
            faces.append([int(m.split("/")[1]) - 1 for m in p[1:]])
    im = Image.new("L", taille, 0)
    dessin = ImageDraw.Draw(im)
    w, h = taille
    for f in faces:
        dessin.polygon([(uvs[i][0] * (w - 1), (1.0 - uvs[i][1]) * (h - 1)) for i in f],
                       fill=255)
    im = im.filter(ImageFilter.GaussianBlur(1.0))
    return np.asarray(im, dtype=np.float64) / 255.0


def metal(haut, alpha):
    """Rend son métal nu à la zone masquée : la luminance d'origine module un
       dégradé bleui (creux) vers paille (pleine lumière), gain borné pour ne
       saturer aucun canal (un canal écrêté ferait retomber la saturation sous
       le seuil de retint.py)."""
    lum = 0.299 * haut[..., 0] + 0.587 * haut[..., 1] + 0.114 * haut[..., 2]
    zone = alpha > 0.5
    gain = np.clip(lum / max(float(lum[zone].mean()), 1e-4), *METAL_GAIN)
    part = np.clip(gain, 0.0, 1.0)[..., None]
    teinte = METAL_BLEUI + (METAL_PAILLE - METAL_BLEUI) * part
    cible = teinte * gain[..., None]
    return haut + (cible - haut) * alpha[..., None]


def grain(forme):
    """Bruit multiplicatif doux, adouci pour rester sous le seuil du criard."""
    alea = np.random.default_rng(GRAINE).uniform(-1.0, 1.0, forme)
    doux = Image.fromarray(((alea + 1.0) * 127.5).astype(np.uint8))
    doux = np.asarray(doux.filter(ImageFilter.GaussianBlur(1.0)), dtype=np.float64)
    return 1.0 + GRAIN * (doux / 127.5 - 1.0)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAUT
    relief = sys.argv[2] if len(sys.argv) > 2 else RELIEF_DEFAUT
    dst = sys.argv[3] if len(sys.argv) > 3 else DST_DEFAUT
    moteur = sys.argv[4] if len(sys.argv) > 4 else None

    base = Image.open(src).convert("RGB")
    taille = Image.open(relief).size
    haut = np.asarray(base.resize(taille, Image.LANCZOS), dtype=np.float64) / 255.0

    if moteur is not None:
        haut = metal(haut, masque_uv(moteur, taille))
    else:
        print("make_fond : pas d'OBJ moteur, la turbine sera peinte couleur livrée")

    div = courbure(relief, taille)
    facteur = 1.0 + np.where(div > 0.0, ARETE, CREUX) * div
    facteur = facteur * grain(div.shape)
    out = np.clip(haut * facteur[..., None], 0.0, 1.0)

    Image.fromarray((out * 255).astype(np.uint8)).save(dst)
    marque = (np.abs(div) > 0.02).mean() * 100.0
    print(f"fond écrit -> {dst}  ({taille[0]}x{taille[1]}, "
          f"{marque:.0f}% des texels marqués par le relief)")


if __name__ == "__main__":
    main()
