"""
retint.py
Repeint le fuselage de l'Alouette II (le "métal" gris et l'accent orange de la
poutre) dans une couleur cible, en conservant l'ombrage d'origine. Facteur
commun aux quatre livrées du fuselage (make_gendarmerie.py, make_blanche.py,
make_armeedeterre.py, make_protectioncivile.py), qui ne diffèrent que par leur
couleur cible et, pour la Protection civile, par un plafonnement du gain.

Auteur : O. Booklage
Licence : GPL v2
"""

import os

import numpy as np
from PIL import Image

FORCE_AO = 0.6  # 0 = aucune occlusion, 1 = creux poussés au noir

# Turbine : elle est peinte comme le reste, puis ramenée à cette luminance
# moyenne. Elle ressort donc presque noire, mais dans la nuance de sa livrée,
# bleutée sur la Gendarmerie, verdâtre sur l'armée de terre. Assombrir VERS une
# luminance et non PAR un facteur est ce qui rend les quatre livrées
# comparables : leurs couleurs de départ vont du bleu nuit au blanc cassé, un
# facteur unique donnerait une turbine noire d'un côté et grise de l'autre.
LUM_TURBINE = 0.10


def occlusion(out, chemin, force=FORCE_AO):
    """Multiplie l'image par la carte d'occlusion ambiante cuite (voir cuire_ao.py).

       La carte est cuite au double de la résolution de l'atlas : on la réduit
       ici, ce qui la suréchantillonne au passage. Les texels que le dépliage
       n'utilise pas ressortent noirs de la cuisson ; ils sont ramenés à 1 pour
       ne pas noircir des zones peintes voisines au filtrage.

       Zone épargnée : si un fichier "texture-ao-epargne.obj" se trouve à côté de
       la carte, sa zone UV n'est pas assombrie. Il sert aux tubes de la cabine
       (montants de porte, structure, barres) : ce sont des cylindres à peu de
       facettes, la cuisson y écrit un anneau par facette et l'occlusion en fait
       des bandes claires et sombres, très visibles en vue cockpit. Les tôles
       plates, elles, gagnent à garder leur occlusion."""
    hauteur, largeur = out.shape[:2]
    carte = Image.open(chemin).convert("L").resize((largeur, hauteur), Image.LANCZOS)
    ao = np.asarray(carte).astype(np.float32) / 255.0
    ao = np.where(ao < 0.05, 1.0, ao)
    epargne = os.path.join(os.path.dirname(chemin) or ".", "texture-ao-epargne.obj")
    if os.path.exists(epargne):
        from make_fond import masque_uv
        zone = masque_uv(epargne, (largeur, hauteur)).astype(np.float32)
        ao = ao + (1.0 - ao) * zone
        print(f"   occlusion épargnée sur {(zone > 0.5).mean() * 100:.1f}% de l'atlas")
    return out * (1.0 - force * (1.0 - ao))[..., None]


def retint(src, dst, target_hex, gain_clip=None):
    """Repeint src vers dst dans la couleur target_hex (tuple RGB 0..255).

       Masque des pixels à repeindre : le métal gris (faiblement saturé) et
       l'accent orange de la poutre, distingué du rouge de la cocarde (l'orange
       a un vert nettement supérieur au bleu, alors que le rouge de la cocarde a
       un vert proche du bleu). Le gain (luminance / luminance moyenne du
       masque) module la couleur cible pixel à pixel : la teinte moyenne des
       pixels repeints tombe exactement sur la cible, les creux deviennent plus
       sombres et les hautes lumières plus claires, sans changer de nuance.
       gain_clip, s'il est donné, borne ce gain (min, max) : sans borne haute,
       les reflets brillants du métal peuvent saturer un canal avant les
       autres et délaver la teinte (cas de la Protection civile, en rouge)."""
    img = np.asarray(Image.open(src).convert("RGB")).astype(np.float32) / 255.0
    r, g, b = img[..., 0], img[..., 1], img[..., 2]

    mx = np.maximum(np.maximum(r, g), b)
    mn = np.minimum(np.minimum(r, g), b)
    sat = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0.0)
    lum = 0.299 * r + 0.587 * g + 0.114 * b

    neutral = sat < 0.18
    orange = (sat >= 0.18) & (r > g) & (g - b > 0.12) & (r - b > 0.25)
    neutral = neutral | orange

    target = np.array(target_hex, dtype=np.float32) / 255.0
    mean_l = float(lum[neutral].mean())
    gain = lum / mean_l
    if gain_clip is not None:
        gain = np.clip(gain, gain_clip[0], gain_clip[1])

    out = img.copy()
    out[..., 0] = np.where(neutral, target[0] * gain, r)
    out[..., 1] = np.where(neutral, target[1] * gain, g)
    out[..., 2] = np.where(neutral, target[2] * gain, b)

    # Occlusion ambiante, si la carte est posée à côté de la SOURCE (le fond
    # fabriqué par make_fond.py, dans le dossier de travail). La carte cuite est
    # versionnée dans Models/cuisson/ ; la chaîne la recopie à côté du fond, avec
    # texture-ao-epargne.obj s'il y a une zone à épargner. Le message imprimé
    # ci-dessous dit si elle a bien été trouvée.
    # Elle s'applique après le repeint, et à toute l'image (les marquages sont
    # dans l'ombre au même titre que la tôle).
    carte_ao = os.path.join(os.path.dirname(src) or ".", "texture-ao.png")
    avec_ao = os.path.exists(carte_ao)
    if avec_ao:
        out = occlusion(out, carte_ao)

    # Turbine : si l'OBJ de sa zone UV est posé à côté de la source, on ramène
    # cette zone à LUM_TURBINE en multipliant ses trois canaux par le même
    # facteur, ce qui assombrit sans toucher à la teinte.
    obj_turbine = os.path.join(os.path.dirname(src) or ".", "texture-turbine.obj")
    if os.path.exists(obj_turbine):
        from make_fond import masque_uv
        zone = masque_uv(obj_turbine, (out.shape[1], out.shape[0]))
        # Fondu resserré sur un demi-pixel de part et d'autre du bord : un texel
        # assombri à moitié garde trop de la couleur de livrée et ressort clair
        # sur le pourtour de la turbine, très visible sur la blanche.
        zone = np.clip((zone - 0.35) / 0.30, 0.0, 1.0)
        coeur = zone > 0.5
        lum = 0.299 * out[..., 0] + 0.587 * out[..., 1] + 0.114 * out[..., 2]
        moyenne = float(lum[coeur].mean()) if coeur.any() else 0.0
        if moyenne > 1e-4:
            facteur = 1.0 + (LUM_TURBINE / moyenne - 1.0) * zone
            out = out * facteur[..., None]
            print(f"   turbine ramenée à {LUM_TURBINE:.2f} de luminance "
                  f"(depuis {moyenne:.2f}, {coeur.mean() * 100:.1f}% de l'atlas)")

    out = np.clip(out, 0.0, 1.0)
    Image.fromarray((out * 255).astype("uint8")).save(dst)
    print(f"livrée écrite -> {dst}  ({neutral.mean() * 100:.0f}% des pixels repeints"
          f"{', occlusion appliquée' if avec_ao else ''})")
