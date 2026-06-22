#!/usr/bin/env python3
"""
check_tailrotor.py
Contrôle automatique d'une texture de rotor de queue : vérifie que le quart des
pales (haut-droit) n'est plus quasi noir, et qu'un quart de référence (le disque,
en bas à gauche) garde de la couleur. Sert de garde-fou après régénération.

Usage : python3 tools/livree/check_tailrotor.py <texture.png> [--zebra]
Renvoie 0 si conforme, 1 sinon.

Auteur : O. Booklage
Licence : GPL v2
"""

import sys

import numpy as np
from PIL import Image


def quart(arr, droite, bas):
    """Renvoie un quart de l'image (moitié gauche/droite x moitié haut/bas)."""
    h, w, _ = arr.shape
    xs = slice(w // 2, w) if droite else slice(0, w // 2)
    ys = slice(h // 2, h) if bas else slice(0, h // 2)
    return arr[ys, xs]


def main():
    chemin = sys.argv[1]
    zebra = "--zebra" in sys.argv[2:]
    arr = np.asarray(Image.open(chemin).convert("RGB"), dtype=np.float32)

    pale = quart(arr, droite=True, bas=False)      # quart haut-droit = pales
    disque = quart(arr, droite=False, bas=True)    # quart bas-gauche = disque (référence)

    lum_pale = float(pale.mean())
    print(f"[check] luminance moyenne du quart pale : {lum_pale:.1f}")
    if lum_pale < 60.0:
        print("[check] ÉCHEC : quart pale encore trop sombre")
        return 1

    # Le disque (bas-gauche) doit garder ses arcs jaunes : on en compte les pixels.
    # Garde-fou contre une régression qui écraserait le disque (sa moyenne est
    # quasi grise, les arcs étant fins : on compte donc les pixels jaunes vifs).
    jaune_disque = (disque[:, :, 0] > 120) & (disque[:, :, 1] > 90) & (disque[:, :, 2] < 90)
    print(f"[check] pixels jaunes du disque : {int(jaune_disque.sum())}")
    if jaune_disque.sum() < 200:
        print("[check] ÉCHEC : le quart disque a perdu ses arcs jaunes (régression)")
        return 1

    if zebra:
        # Présence de rouge ET de jaune dans le quart pale.
        rouge = (pale[:, :, 0] > 150) & (pale[:, :, 1] < 90) & (pale[:, :, 2] < 90)
        jaune = (pale[:, :, 0] > 150) & (pale[:, :, 1] > 130) & (pale[:, :, 2] < 90)
        print(f"[check] pixels rouges={int(rouge.sum())} jaunes={int(jaune.sum())}")
        if rouge.sum() < 50 or jaune.sum() < 50:
            print("[check] ÉCHEC : zébrures jaune/rouge absentes")
            return 1

    print("[check] OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
