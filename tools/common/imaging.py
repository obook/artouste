"""
imaging.py
Fonctions de manipulation d'image partagées par les générateurs de textures
procédurales : un bruit gaussien flouté réutilisable (noise()), dupliqué à
l'identique entre generer_facade.py et helipad/make_texture.py avant cette
factorisation.

Auteur : O. Booklage
Licence : GPL v2
"""

from PIL import Image, ImageFilter


def noise(width, height, sigma, blur=0.0):
    """Bruit gaussien en niveaux de gris (image PIL 'L', taille width x height),
       flouté si blur > 0. Sert de grain fin indépendant par pixel (plâtre,
       béton) aux générateurs de texture procédurale."""
    img = Image.effect_noise((width, height), sigma).convert("L")
    if blur > 0.0:
        img = img.filter(ImageFilter.GaussianBlur(blur))
    return img
