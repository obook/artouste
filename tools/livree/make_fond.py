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

Avec un quatrième argument (l'OBJ des pièces cylindriques : montants, barres,
structure), leur zone UV est ÉPARGNÉE par le tamponnage du relief. La carte a
été cuite avec un biseau sur les arêtes vives : sur une tôle plate elle redonne
les lignes de tôle, mais un tube est un cylindre à peu de facettes, chaque arête
de facette y reçoit son biseau, et le tamponnage transforme ces anneaux en
bandes claires et sombres. Le tube en ressort tigré, bien visible en vue
cockpit. L'OBJ s'exporte en écartant les tôles de peau :

    ./build/bin/model_probe assets/models/Alouette-II/Models/alouette.ac \\
        /tmp/tubes.obj "fuselage,cache,hdr,blur,disc"

La turbine, elle, n'est plus traitée ici : elle est teintée et assombrie par
retint.py, qui connaît la couleur de la livrée en cours (voir texture-turbine.obj).

Usage : python3 tools/livree/make_fond.py [src] [relief] [dst] [tubes.obj]
Sortie par défaut : assets/models/Alouette-II/Models/texture-fond.png

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

SRC_DEFAUT = "assets/models/Alouette-II/Models/texture.png"
# Les deux cartes cuites vivent dans un SOUS-DOSSIER "cuisson" et non à côté de
# texture.png : le moteur associe automatiquement X.png à X-relief.png, une carte
# de relief posée à côté de l'atlas serait donc chargée pour le fuselage, ce qu'on
# ne veut pas (21 Mo de mémoire vidéo pour un gain invisible à plus de 5 m).
RELIEF_DEFAUT = "assets/models/Alouette-II/Models/cuisson/texture-relief.png"
DST_DEFAUT = "assets/models/Alouette-II/Models/texture-fond.png"

ARETE = 1.3    # gain de luminance au sommet d'une arête convexe
CREUX = 1.6    # perte de luminance au fond d'une rainure
COURBURE_MAX = 0.15  # écrêtage de la divergence : au-delà, l'effet n'augmente plus
GRAIN = 0.025  # amplitude du grain, en fraction de la luminance
GRAINE = 313   # graine du grain : SA 313, et surtout reproductible

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
    tubes = sys.argv[4] if len(sys.argv) > 4 else None

    base = Image.open(src).convert("RGB")
    taille = Image.open(relief).size
    haut = np.asarray(base.resize(taille, Image.LANCZOS), dtype=np.float64) / 255.0

    div = courbure(relief, taille)
    if tubes is not None:
        epargne = masque_uv(tubes, taille)
        div = div * (1.0 - epargne)
        print(f"pièces cylindriques épargnées par le relief "
              f"({(epargne > 0.5).mean() * 100:.1f}% de l'atlas)")
    facteur = 1.0 + np.where(div > 0.0, ARETE, CREUX) * div
    facteur = facteur * grain(div.shape)
    out = np.clip(haut * facteur[..., None], 0.0, 1.0)

    Image.fromarray((out * 255).astype(np.uint8)).save(dst)
    marque = (np.abs(div) > 0.02).mean() * 100.0
    print(f"fond écrit -> {dst}  ({taille[0]}x{taille[1]}, "
          f"{marque:.0f}% des texels marqués par le relief)")


if __name__ == "__main__":
    main()
