#!/usr/bin/env python3
"""
appliquer_ao.py
Produit une texture assombrie par l'occlusion ambiante, à partir de la texture
d'origine et de la carte cuite par cuire_ao.py. Sert à l'intérieur de cabine et
à la planche de bord.

L'original n'est jamais modifié : contrairement aux quatre livrées du fuselage,
qui sont des fichiers fabriqués, interior.png et panel.png sont des textures
source du modèle FlightGear. On écrit donc à côté, en "-occlusion.png", que le
moteur charge comme texture de rechange (voir LoadedHelicopter.cpp) et qui
laisse la texture d'origine en place si le fichier manque.

Chaîne complète, depuis la racine du dépôt :

    ./build/bin/model_probe assets/models/Alouette-II/Models/Interior/interior.ac \\
        /tmp/interior.obj "hdr,blur,disc,flotteur,barre,roue"
    ./build/bin/model_probe assets/models/Alouette-II/Models/alouette.ac \\
        /tmp/occulteur.obj "hdr,blur,disc,flotteur,barre,roue,verriere,vitreporte"
    blender -b --factory-startup -P tools/livree/cuire_ao.py -- \\
        /tmp/interior.obj assets/models/Alouette-II/Models/Interior/interior-ao.png \\
        1024x512 /tmp/occulteur.obj 0.30
    PYTHONPATH=tools/livree tools/.venv/bin/python tools/livree/appliquer_ao.py

Pour la planche de bord, sans occulteur et à portée plus courte encore, le
tableau ne faisant que 0,54 m :

    ./build/bin/model_probe assets/models/Alouette-II/Models/Interior/Panel/panel.ac \\
        /tmp/panel.obj "blur,disc,sur"
    blender -b --factory-startup -P tools/livree/cuire_ao.py -- \\
        /tmp/panel.obj PANEL/panel-ao.png 1024 - 0.10
    PYTHONPATH=tools/livree tools/.venv/bin/python tools/livree/appliquer_ao.py \\
        PANEL/panel.png PANEL/panel-ao.png PANEL/panel-occlusion.png

La portée n'est pas un détail : à 2,5 m, celle qui convient au fuselage vu de
l'extérieur, une cabine de 2,3 m de long se comporte comme une pièce fermée et
la cuisson rend un intérieur uniformément noir (moyenne 0,25 au lieu de 0,90).
On cherche ici les ombres de contact des angles, des nervures et des
encastrements de cadran, pas l'enfermement.

Les capots pare-soleil (sur1..sur6) sont écartés à la cuisson comme au rendu :
cuire l'ombre d'une pièce qui ne s'affiche pas donnerait des bandes sombres
inexplicables en travers du tableau.

Usage : python3 tools/livree/appliquer_ao.py [src] [ao] [dst]

Auteur : O. Booklage
Date : août 2026
Licence : GPL v2
"""

import sys

import numpy as np
from PIL import Image

from retint import occlusion

DOSSIER = "assets/models/Alouette-II/Models/Interior/"
SRC_DEFAUT = DOSSIER + "interior.png"
AO_DEFAUT = DOSSIER + "interior-ao.png"
DST_DEFAUT = DOSSIER + "interior-occlusion.png"


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAUT
    ao = sys.argv[2] if len(sys.argv) > 2 else AO_DEFAUT
    dst = sys.argv[3] if len(sys.argv) > 3 else DST_DEFAUT

    img = np.asarray(Image.open(src).convert("RGB")).astype(np.float32) / 255.0
    out = np.clip(occlusion(img, ao), 0.0, 1.0)
    Image.fromarray((out * 255).astype("uint8")).save(dst)

    assombrissement = (img - out).mean() * 255.0
    print(f"texture écrite -> {dst}  (assombrissement moyen "
          f"{assombrissement:.1f} niveaux sur 255)")


if __name__ == "__main__":
    main()
