#!/usr/bin/env python3
"""
feux_capbreton.py
Fabrique les trois feux d'entrée du port de Capbreton, d'après leurs fiches
pharologiques (ARLHS FRA-808 et FRA-809, ibiblio Lighthouses of France:
Aquitaine ; Wikipédia "Phares de Capbreton").

  feu-vert-tour    tour en pierre de 1948, ~8 m, sur l'estacade sud.
                   Inactive depuis que le feu est passé sur le pylône avant.
                   Fût pierre, galerie débordante à garde-corps blanc,
                   lanterne verte.
  feu-vert-pylone  pylône blanc, ~40 m à l'ouest de la tour, même estacade.
                   Porte le feu actif : 2 éclats verts / 6 s, focale 9 m.
  feu-rouge-tour   tour cylindrique en béton de 1975, ~6 m, sur la jetée nord.
                   Fût blanc, bande rouge sous la galerie, lanterne rouge,
                   garde-corps métal nu, contrefort triangulaire côté terre.
                   2 éclats rouges / 6 s, portée 14 milles.

Les tours sont volontairement basses : leur portée vient de la hauteur des
jetées, pas de la leur. Ne pas les grandir.

Les couleurs passent par un ATLAS de teintes plates : monument.frag ne lit
qu'une texture, ni la couleur de matériau AC3D ni celle des sommets. Chaque face
pointe donc sur la pastille de sa couleur. Sans cela le modèle sort tout gris.

Ce que ce modèle ne porte pas : l'inscription "CAPBRETON" en lettres rouges sur
la face ouest de la tour rouge.

Usage : python3 tools/monuments/feux_capbreton.py
Sortie : assets/models/monuments/capbreton/feu-*.ac

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import sys
from pathlib import Path

from PIL import Image

RACINE = Path(__file__).resolve().parent.parent.parent
SORTIE = RACINE / "assets" / "models" / "monuments" / "capbreton"

# Palette de la signalisation maritime, telle que la fiche la donne.
COULEURS = {
    "pierre": (0.62, 0.59, 0.53),   # gris-beige, pierre apparente
    "blanc": (0.92, 0.92, 0.90),
    "rouge": (0.78, 0.10, 0.10),    # rouge feu
    "vert": (0.05, 0.45, 0.20),     # vert feu
    "metal": (0.55, 0.56, 0.58),    # garde-corps de la tour rouge, métal nu
    "verre": (0.75, 0.85, 0.80),
}
NOMS = list(COULEURS)
SEGMENTS = 16
PASTILLE_PX = 32          # côté d'une pastille dans l'atlas
ATLAS = "feux-teintes.png"


def ecrire_atlas(chemin):
    """Une pastille carrée par teinte, alignées en bande."""
    image = Image.new("RGB", (PASTILLE_PX * len(NOMS), PASTILLE_PX))
    for i, cle in enumerate(NOMS):
        r, v, b = COULEURS[cle]
        pastille = Image.new("RGB", (PASTILLE_PX, PASTILLE_PX),
                             (int(r * 255), int(v * 255), int(b * 255)))
        image.paste(pastille, (i * PASTILLE_PX, 0))
    image.save(chemin)
    return image.size


def uv_de(mat):
    """Centre de la pastille : on échantillonne loin des bords, le mipmap
       mélangeant les teintes voisines si l'on s'en approche."""
    return ((NOMS.index(mat) + 0.5) / len(NOMS), 0.5)


def cylindre(sommets, faces, cx, cz, y0, y1, r0, r1, mat, ferme_haut=True):
    """Tronc de cône vertical, plus son disque supérieur."""
    base = len(sommets)
    for i in range(SEGMENTS):
        a = 2.0 * math.pi * i / SEGMENTS
        sommets.append((cx + r0 * math.cos(a), y0, cz + r0 * math.sin(a)))
        sommets.append((cx + r1 * math.cos(a), y1, cz + r1 * math.sin(a)))
    for i in range(SEGMENTS):
        j = (i + 1) % SEGMENTS
        faces.append(((base + 2 * i, base + 2 * i + 1, base + 2 * j + 1, base + 2 * j), mat))
    if ferme_haut:
        faces.append((tuple(base + 2 * i + 1 for i in range(SEGMENTS)), mat))


def cone(sommets, faces, cx, cz, y0, y1, r, mat):
    """Toit conique, pour couronner une lanterne."""
    base = len(sommets)
    sommets.append((cx, y1, cz))
    for i in range(SEGMENTS):
        a = 2.0 * math.pi * i / SEGMENTS
        sommets.append((cx + r * math.cos(a), y0, cz + r * math.sin(a)))
    for i in range(SEGMENTS):
        j = (i + 1) % SEGMENTS
        faces.append(((base, base + 1 + i, base + 1 + j), mat))


def garde_corps(sommets, faces, cx, cz, y, r, hauteur, mat):
    """Anneau mince : la lisse du garde-corps, vue de loin."""
    cylindre(sommets, faces, cx, cz, y + hauteur * 0.75, y + hauteur, r, r, mat,
             ferme_haut=False)
    cylindre(sommets, faces, cx, cz, y, y + hauteur * 0.1, r, r, mat, ferme_haut=False)


def ecrire_ac(chemin, sommets, faces, nom):
    """Chaque face reçoit ses propres sommets : l'UV porte la couleur, deux faces
       de teintes différentes ne peuvent donc pas partager un sommet."""
    positions, uv, refs = [], [], []
    for face, mat in faces:
        debut = len(positions)
        u, v = uv_de(mat)
        for k in face:
            positions.append(sommets[k])
            uv.append((u, v))
        refs.append(tuple(range(debut, debut + len(face))))
    with open(chemin, "w", encoding="utf-8") as f:
        f.write("AC3Db\n")
        f.write("MATERIAL \"teintes\" rgb 1 1 1 amb 1 1 1 emis 0 0 0 "
                "spec 0.2 0.2 0.2 shi 32 trans 0\n")
        f.write(f"OBJECT world\nkids 1\nOBJECT poly\nname \"{nom}\"\n")
        f.write(f"texture \"{ATLAS}\"\n")
        f.write(f"numvert {len(positions)}\n")
        for x, y, z in positions:
            f.write(f"{x:.4f} {y:.4f} {z:.4f}\n")
        f.write(f"numsurf {len(refs)}\n")
        for face in refs:
            f.write("SURF 0x30\nmat 0\n")
            f.write(f"refs {len(face)}\n")
            for k in face:
                f.write(f"{k} {uv[k][0]:.5f} {uv[k][1]:.5f}\n")
        f.write("kids 0\n")


def tour_verte():
    """Tour en pierre de 1948 : fût, galerie blanche, lanterne verte. 8 m."""
    s, f = [], []
    cylindre(s, f, 0, 0, 0.0, 6.0, 1.15, 1.00, "pierre", ferme_haut=False)
    cylindre(s, f, 0, 0, 6.0, 6.3, 1.55, 1.55, "blanc")          # galerie débordante
    garde_corps(s, f, 0, 0, 6.3, 1.45, 0.95, "blanc")
    cylindre(s, f, 0, 0, 6.3, 7.4, 0.65, 0.65, "verre")          # lanterne
    cylindre(s, f, 0, 0, 7.4, 7.6, 0.72, 0.72, "vert", ferme_haut=False)
    cone(s, f, 0, 0, 7.6, 8.0, 0.72, "vert")
    return s, f


def pylone_vert():
    """Pylône blanc du feu avant, plate-forme et garde-corps. 9 m."""
    s, f = [], []
    cylindre(s, f, 0, 0, 0.0, 7.4, 0.40, 0.32, "blanc", ferme_haut=False)
    cylindre(s, f, 0, 0, 7.4, 7.6, 1.15, 1.15, "blanc")
    garde_corps(s, f, 0, 0, 7.6, 1.05, 0.85, "blanc")
    cylindre(s, f, 0, 0, 7.6, 8.6, 0.55, 0.55, "verre")
    cone(s, f, 0, 0, 8.6, 9.0, 0.65, "vert")
    return s, f


def tour_rouge():
    """Tour béton de 1975 : fût blanc, bande rouge, lanterne rouge, contrefort
       triangulaire côté terre. 6 m au sommet du garde-corps."""
    s, f = [], []
    cylindre(s, f, 0, 0, 0.0, 3.6, 1.10, 1.10, "blanc", ferme_haut=False)
    cylindre(s, f, 0, 0, 3.6, 4.4, 1.14, 1.14, "rouge", ferme_haut=False)  # bande rouge
    cylindre(s, f, 0, 0, 4.4, 4.65, 1.50, 1.50, "blanc")                   # galerie
    garde_corps(s, f, 0, 0, 4.65, 1.40, 0.90, "metal")
    cylindre(s, f, 0, 0, 4.65, 5.60, 0.62, 0.62, "verre")
    cylindre(s, f, 0, 0, 5.60, 5.80, 0.70, 0.70, "rouge", ferme_haut=False)
    cone(s, f, 0, 0, 5.80, 6.20, 0.70, "rouge")
    # Contrefort en triangle rectangle, accolé côté terre (est), hypoténuse au
    # large. X est, Z sud : on l'adosse en +X.
    base = len(s)
    for z in (-0.95, 0.95):
        s.extend([(0.95, 0.0, z), (3.20, 0.0, z), (0.95, 2.80, z)])
    f.extend([((base, base + 1, base + 2), "blanc"),
              ((base + 3, base + 5, base + 4), "blanc"),
              ((base, base + 3, base + 4, base + 1), "blanc"),
              ((base + 1, base + 4, base + 5, base + 2), "blanc"),
              ((base, base + 2, base + 5, base + 3), "blanc")])
    return s, f


def main():
    SORTIE.mkdir(parents=True, exist_ok=True)
    taille = ecrire_atlas(SORTIE / ATLAS)
    print(f"[feux] {ATLAS} : {taille[0]}x{taille[1]}, {len(NOMS)} teintes")
    for nom, fabrique in (("feu-vert-tour", tour_verte),
                          ("feu-vert-pylone", pylone_vert),
                          ("feu-rouge-tour", tour_rouge)):
        sommets, faces = fabrique()
        chemin = SORTIE / f"{nom}.ac"
        ecrire_ac(chemin, sommets, faces, nom)
        haut = max(y for _, y, _ in sommets)
        print(f"[feux] {chemin.name} : {len(sommets)} sommets, {len(faces)} faces, "
              f"{haut:.1f} m de haut, {chemin.stat().st_size / 1024:.0f} ko")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
