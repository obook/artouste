#!/usr/bin/env python3
"""
passerelle.py
Fabrique la passerelle en bois de l'estacade de Capbreton : le platelage sur
pilotis qui court le long de la jetée sud, côté chenal.

Ce que la mesure donne, et non la fiche :

  ecart     8,2 m du côté chenal de l'axe du môle, relevé sur la BD ORTHO par la
            signature des traverses. Le bois est rayé tous les 25 cm : une
            variance locale calculée le long de l'axe pique là où il est.
  largeur   3,3 m, largeur de la bande où cette variance tient plus de la
            moitié de son maximum.

La hauteur du platelage n'est pas mesurable : le laser aéroporté traverse les
lames et se classe au sol. On la pose à celle de la promenade du môle, ce qui
est cohérent, on passe de l'une à l'autre de plain-pied.

Le bois est une texture de synthèse, lames brun-gris patinées : l'orthophoto
n'offre que 16 pixels en travers du platelage, trop peu pour en tirer une tuile.

Usage : python3 tools/monuments/passerelle.py
Sortie : assets/models/monuments/capbreton/passerelle.{ac,png}

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

RACINE = Path(__file__).resolve().parent.parent.parent
SORTIE = RACINE / "assets" / "models" / "monuments" / "capbreton"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from estacade import AXE_SUD, M_PAR_DEG_LAT, metres, normales_de_section

TEXTURE = "bois.png"

ECART_M = -8.2        # côté chenal ; le signe suit la perpendiculaire de l'axe
LARGEUR_M = 3.3
DESSUS_M = 3.4        # hauteur du platelage, celle de la promenade du môle
EPAISSEUR_M = 0.35    # sommiers sous les lames
PAS_PILOTIS_M = 4.0
PILOTIS_COTE_M = 0.28
GARDE_CORPS_M = 1.05
LISSE_COTE_M = 0.07
POTEAU_COTE_M = 0.05

# Une tuile de texture porte quatre lames de 25 cm, soit 1 m de passerelle.
TUILE_M = 1.0


def ecrire_texture(chemin):
    """Lames transversales, brun-gris patiné, veinées d'une nuance par lame."""
    px = 256
    image = Image.new("RGB", (px, px), (120, 106, 92))
    dessin = ImageDraw.Draw(image)
    lames = 4
    for i in range(lames):
        y0 = i * px // lames
        y1 = (i + 1) * px // lames
        # Chaque lame tire une teinte propre, pour que le platelage ne soit pas
        # un aplat : le bois de mer grise irrégulièrement.
        ton = 96 + (i * 37) % 46
        dessin.rectangle([0, y0, px, y1 - 2], fill=(ton + 22, ton + 8, ton - 6))
        dessin.rectangle([0, y1 - 2, px, y1], fill=(58, 50, 43))   # joint entre lames
    image.save(chemin)
    return image.size


def axe_decale(axe_m, ecart):
    """L'axe du môle translaté de ecart mètres sur sa perpendiculaire."""
    perp = normales_de_section(axe_m)
    return [(x + perp[i][0] * ecart, z + perp[i][1] * ecart)
            for i, (x, z) in enumerate(axe_m)]


def rallonger(axe, pas):
    """Rééchantillonne l'axe à pas constant : les pilotis doivent tomber à
       intervalle régulier, pas aux sommets du relevé."""
    total = [0.0]
    for i in range(1, len(axe)):
        total.append(total[-1] + math.hypot(axe[i][0] - axe[i - 1][0], axe[i][1] - axe[i - 1][1]))
    sorties, d = [], 0.0
    while d <= total[-1]:
        i = max(k for k in range(len(total)) if total[k] <= d)
        if i + 1 < len(axe):
            f = (d - total[i]) / max(total[i + 1] - total[i], 1e-6)
            sorties.append((axe[i][0] + f * (axe[i + 1][0] - axe[i][0]),
                            axe[i][1] + f * (axe[i + 1][1] - axe[i][1])))
        else:
            sorties.append(axe[-1])
        d += pas
    return sorties


def boite(sommets, faces, centre, demi):
    """Pavé droit aligné sur les axes, six faces."""
    cx, cy, cz = centre
    dx, dy, dz = demi
    base = len(sommets)
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                sommets.append((cx + sx * dx, cy + sy * dy, cz + sz * dz))
    # indices des 8 coins : bit 2 = x, bit 1 = y, bit 0 = z
    for q in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1),
              (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        faces.append(tuple(base + k for k in q))


def barre(sommets, faces, a, b, demi_cote):
    """Barreau droit entre deux points, de section carrée ORIENTÉE sur son axe.
       Une boîte alignée sur les axes du monde faisait, pour un barreau en
       diagonale, un pavé de plusieurs mètres de côté : la rambarde en devenait
       un bandeau plein aussi large que le môle."""
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    longueur = math.hypot(math.hypot(dx, dy), dz)
    if longueur < 1e-6:
        return
    ux, uy, uz = dx / longueur, dy / longueur, dz / longueur
    # Deux perpendiculaires à l'axe du barreau.
    px, py, pz = (-uz, 0.0, ux) if abs(uy) > 0.9 else (-uz, 0.0, ux)
    n = math.hypot(px, pz) or 1.0
    px, pz = px / n, pz / n
    qx, qy, qz = (uy * pz - uz * py, uz * px - ux * pz, ux * py - uy * px)
    base = len(sommets)
    for bout in (a, b):
        for sp in (-1, 1):
            for sq in (-1, 1):
                sommets.append((bout[0] + (px * sp + qx * sq) * demi_cote,
                                bout[1] + (py * sp + qy * sq) * demi_cote,
                                bout[2] + (pz * sp + qz * sq) * demi_cote))
    for q in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1),
              (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        faces.append(tuple(base + k for k in q))


def construire(axe_m):
    stations = rallonger(axe_m, PAS_PILOTIS_M)
    demi = 0.5 * LARGEUR_M
    perp = normales_de_section(stations)
    sommets, faces = [], []

    # Platelage : un ruban de segments, dessus, dessous et flancs.
    haut, bas = DESSUS_M, DESSUS_M - EPAISSEUR_M
    for i in range(len(stations) - 1):
        a, b = stations[i], stations[i + 1]
        pa, pb = perp[i], perp[i + 1]
        coins = []
        for y in (haut, bas):
            coins += [(a[0] + pa[0] * demi, y, a[1] + pa[1] * demi),
                      (a[0] - pa[0] * demi, y, a[1] - pa[1] * demi),
                      (b[0] - pb[0] * demi, y, b[1] - pb[1] * demi),
                      (b[0] + pb[0] * demi, y, b[1] + pb[1] * demi)]
        base = len(sommets)
        sommets.extend(coins)
        faces.extend([(base, base + 1, base + 2, base + 3),               # dessus
                      (base + 4, base + 7, base + 6, base + 5),           # dessous
                      (base, base + 3, base + 7, base + 4),               # flanc
                      (base + 1, base + 5, base + 6, base + 2)])          # flanc

    # Pilotis sous chaque station, poteaux de garde-corps deux fois plus serrés :
    # à quatre mètres ils se confondaient de loin avec les lisses et la rambarde
    # se lisait en panneau plein.
    for i, (x, z) in enumerate(stations):
        px, pz = perp[i]
        for cote in (-1, 1):
            ex, ez = x + px * demi * cote, z + pz * demi * cote
            boite(sommets, faces, (ex, 0.5 * bas, ez),
                  (0.5 * PILOTIS_COTE_M, 0.5 * bas, 0.5 * PILOTIS_COTE_M))
            for f in (0.0, 0.5):
                if f and i + 1 >= len(stations):
                    continue
                if f:
                    b2, pb = stations[i + 1], perp[i + 1]
                    mx = x + f * (b2[0] - x); mz = z + f * (b2[1] - z)
                    mpx = px + f * (pb[0] - px); mpz = pz + f * (pb[1] - pz)
                else:
                    mx, mz, mpx, mpz = x, z, px, pz
                boite(sommets, faces,
                      (mx + mpx * demi * cote, haut + 0.5 * GARDE_CORPS_M, mz + mpz * demi * cote),
                      (POTEAU_COTE_M, 0.5 * GARDE_CORPS_M, POTEAU_COTE_M))
        # Deux lisses par côté, en barreaux orientés sur leur axe.
        if i + 1 < len(stations):
            b2, pb = stations[i + 1], perp[i + 1]
            for cote in (-1, 1):
                for h in (haut + GARDE_CORPS_M, haut + 0.55 * GARDE_CORPS_M):
                    barre(sommets, faces,
                          (x + px * demi * cote, h, z + pz * demi * cote),
                          (b2[0] + pb[0] * demi * cote, h, b2[1] + pb[1] * demi * cote),
                          0.5 * LISSE_COTE_M)
    return sommets, faces


def plaquer(sommets, face):
    """UV par face, sur les deux axes du monde les moins alignés avec sa
       normale. La projection unique que portait la première version dégénérait
       sur toute face parallèle à elle, d'où l'aplat brun."""
    a, b, c = (sommets[k] for k in face[:3])
    u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    v = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    n = (abs(u[1] * v[2] - u[2] * v[1]), abs(u[2] * v[0] - u[0] * v[2]),
         abs(u[0] * v[1] - u[1] * v[0]))
    axes = (1, 2) if n[0] >= max(n[1], n[2]) else ((0, 2) if n[1] >= n[2] else (0, 1))
    return [(sommets[k][axes[0]] / TUILE_M, sommets[k][axes[1]] / TUILE_M) for k in face]


def ecrire_ac(chemin, sommets, faces, nom):
    with open(chemin, "w", encoding="utf-8") as f:
        f.write("AC3Db\n")
        f.write("MATERIAL \"bois\" rgb 1 1 1 amb 1 1 1 emis 0 0 0 "
                "spec 0.1 0.1 0.1 shi 16 trans 0\n")
        f.write(f"OBJECT world\nkids 1\nOBJECT poly\nname \"{nom}\"\n")
        f.write(f"texture \"{TEXTURE}\"\n")
        f.write(f"numvert {len(sommets)}\n")
        for x, y, z in sommets:
            f.write(f"{x:.4f} {y:.4f} {z:.4f}\n")
        f.write(f"numsurf {len(faces)}\n")
        for face in faces:
            uv = plaquer(sommets, face)
            f.write("SURF 0x30\nmat 0\n")
            f.write(f"refs {len(face)}\n")
            for k, (u, v) in zip(face, uv):
                f.write(f"{k} {u:.4f} {v:.4f}\n")
        f.write("kids 0\n")


def main():
    axe_m, centre = metres(AXE_SUD)
    sommets, faces = construire(axe_decale(axe_m, ECART_M))

    ecart_x = 0.5 * (min(x for x, _, _ in sommets) + max(x for x, _, _ in sommets))
    ecart_z = 0.5 * (min(z for _, _, z in sommets) + max(z for _, _, z in sommets))
    sommets = [(x - ecart_x, y, z - ecart_z) for x, y, z in sommets]
    m_par_deg_lon = M_PAR_DEG_LAT * math.cos(math.radians(centre[1]))
    centre = (centre[0] + ecart_x / m_par_deg_lon, centre[1] - ecart_z / M_PAR_DEG_LAT)

    SORTIE.mkdir(parents=True, exist_ok=True)
    taille = ecrire_texture(SORTIE / TEXTURE)
    chemin = SORTIE / "passerelle.ac"
    ecrire_ac(chemin, sommets, faces, "passerelle")
    longueur = sum(math.hypot(axe_m[i + 1][0] - axe_m[i][0], axe_m[i + 1][1] - axe_m[i][1])
                   for i in range(len(axe_m) - 1))
    print(f"[passerelle] {TEXTURE} : {taille[0]}x{taille[1]}")
    print(f"[passerelle] {chemin.name} : {len(sommets)} sommets, {len(faces)} faces, "
          f"{chemin.stat().st_size / 1024:.0f} ko")
    print(f"[passerelle] {longueur:.0f} m de long, {LARGEUR_M} m de large, "
          f"platelage a {DESSUS_M} m")
    print(f"\n[monuments.txt] ligne a ecrire :\n")
    print(f"{centre[0]:.6f} {centre[1]:.6f} 0.0 0 1 1 0 capbreton/passerelle.ac "
          f"Passerelle de l'estacade\n")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
