"""
paths.py
Chemins partagés par les outils : racine du dépôt et sous-dossiers d'assets,
déduits une fois pour toutes de l'emplacement de ce fichier (tools/common/),
quel que soit le script appelant ou son répertoire de travail. Remplace les
calculs de racine empilant plusieurs os.path.dirname(os.path.abspath(...))
répétés dans chaque script (générateurs de textures, outils de terrain).

Auteur : O. Booklage
Licence : GPL v2
"""

from pathlib import Path


def depot_root():
    """Racine du dépôt (le dossier qui contient tools/, assets/, src/...)."""
    return Path(__file__).resolve().parent.parent.parent


def assets_dir(*parts):
    """Chemin sous assets/, à partir de la racine du dépôt. Le dossier n'est
       pas créé : à l'appelant de le faire (os.makedirs / Path.mkdir) si le
       script s'apprête à y écrire."""
    return depot_root().joinpath("assets", *parts)
