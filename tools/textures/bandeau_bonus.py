#!/usr/bin/env python3
"""Fabrique les bandeaux des sphères de bonus du mode zombie.

Sortie : une image en niveaux de gris par bonus, noir pour le corps de la
sphère, blanc pour le motif. Le motif est répété autour de l'équateur pour
rester lisible sous n'importe quel angle. Le shader garde la teinte de la
sphère là où l'image est noire (voir basic.frag, u_texMix).

Usage : python3 tools/textures/bandeau_bonus.py
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

LARGEUR = 1024   # longitude : un tour complet
HAUTEUR = 512    # latitude : du pôle bas au pôle haut
REPETITIONS = 3  # occurrences du motif autour de l'équateur
REMPLISSAGE = 0.85  # part de la largeur d'un créneau occupée par le motif

POLICE_TEXTE = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
POLICE_SYMBOLES = "/usr/share/fonts/truetype/noto/NotoSansSymbols-ExtraBold.ttf"

SORTIE = Path(__file__).resolve().parents[2] / "assets" / "textures"

CROIX = "croix"  # motif dessiné, sans police (gardé en réserve)

BANDEAUX = {
    "bonus-carburant.png": ("⛽", POLICE_SYMBOLES),  # pompe, U+26FD
    "bonus-sante.png": ("♥", POLICE_TEXTE),          # coeur, U+2665
    "bonus-mort.png": ("☠", POLICE_TEXTE),           # tête de mort, U+2620
}


def police_ajustee(dessin, texte, chemin_police, largeur_cible):
    """Cherche la plus grande taille de police qui tient dans un créneau."""
    taille = 8
    while True:
        essai = ImageFont.truetype(chemin_police, taille + 4)
        gauche, _, droite, _ = dessin.textbbox((0, 0), texte, font=essai)
        if droite - gauche > largeur_cible or taille > 400:
            return ImageFont.truetype(chemin_police, taille)
        taille += 4


def fabriquer_texte(texte, chemin_police):
    """Rend le motif typographique répété, centré sur la bande équatoriale."""
    image = Image.new("L", (LARGEUR, HAUTEUR), 0)
    dessin = ImageDraw.Draw(image)

    pas = LARGEUR // REPETITIONS
    police = police_ajustee(dessin, texte, chemin_police, int(pas * REMPLISSAGE))
    for i in range(REPETITIONS):
        gauche, haut, droite, bas = dessin.textbbox((0, 0), texte, font=police)
        x = i * pas + (pas - (droite - gauche)) // 2 - gauche
        y = HAUTEUR // 2 - (bas - haut) // 2 - haut
        dessin.text((x, y), texte, font=police, fill=255)
    return image


def fabriquer_croix():
    """Rend des croix pleines à branches égales, sur la bande équatoriale."""
    image = Image.new("L", (LARGEUR, HAUTEUR), 0)
    dessin = ImageDraw.Draw(image)

    pas = LARGEUR // REPETITIONS
    cote = int(pas * REMPLISSAGE * 0.6)  # envergure de la croix
    bras = cote // 3                     # épaisseur des branches
    milieu = HAUTEUR // 2
    for i in range(REPETITIONS):
        centre = i * pas + pas // 2
        dessin.rectangle([centre - bras // 2, milieu - cote // 2,
                          centre + bras // 2, milieu + cote // 2], fill=255)
        dessin.rectangle([centre - cote // 2, milieu - bras // 2,
                          centre + cote // 2, milieu + bras // 2], fill=255)
    return image


def main():
    SORTIE.mkdir(parents=True, exist_ok=True)
    for nom, (motif, police) in BANDEAUX.items():
        chemin = SORTIE / nom
        image = fabriquer_croix() if motif == CROIX else fabriquer_texte(motif, police)
        image.save(chemin)
        print(f"écrit : {chemin}")


if __name__ == "__main__":
    main()
