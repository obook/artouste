"""
bati.py
Ce qui distingue le bâti du rocher dans une surface LiDAR, et ce qu'il faut lui
faire subir pour qu'il ressemble à des bâtiments : écrêter les mâts, retirer les
câbles, redresser les arêtes de toit, rebâtir les coupoles.

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import warnings

import numpy as np

SEUIL_BATI = 2.0      # au-dessus du sol nu : c'est du bâti (m)
# Correction maximale du redressement des arêtes. Il n'existe que pour effacer
# le biseau d'un à deux mètres que le MNS laisse au bord d'un toit. Sans borne,
# la médiane déplaçait des points de +24 m et -9 m sur un bâti dense et haut
# (mesuré à La Mongie) : elle hissait au niveau du toit des points restés au sol,
# et creusait des encoches dans les arêtes. Les bâtiments en sortaient déchirés.
CORRECTION_MAX = 2.5
# Seuil du mât. Mesuré au Pic du Midi : entre 30 et 40 m au-dessus du sol nu se
# tiennent encore 173 points, sur une bande de 46 x 15 m, qui sont une terrasse
# posée en bord de falaise (le MNT plonge sous elle). Le mât seul dépasse 50 m.
SEUIL_MAT = 50.0
VOISINS_MIN = 6       # points bâtis attendus dans un carré 5x5 ; en deçà, câble isolé

# Coupoles. Le MNS n'en rend qu'une calotte écrasée : mesuré sur la grande du Pic
# du Midi, 3,2 m de haut pour 14 m de large, là où une demi-sphère en ferait 7.
CHUTE_COUPOLE = 2.0           # descente sous le sommet qui marque le bord (m)
RAYON_COUPOLE = (2.5, 12.0)   # rayons acceptés (m)
REGULARITE_COUPOLE = 2.2      # rapport toléré entre le plus grand et le plus petit rayon


def compte_voisins(masque, cote):
    """Nombre de points marqués dans le carré cote x cote centré sur chaque
       point (image intégrale : une passe, quelle que soit la taille)."""
    rayon = cote // 2
    borde = np.pad(masque.astype(np.int32), rayon)
    cumul = np.zeros((borde.shape[0] + 1, borde.shape[1] + 1), dtype=np.int32)
    cumul[1:, 1:] = borde.cumsum(0).cumsum(1)
    return (cumul[cote:, cote:] - cumul[:-cote, cote:]
            - cumul[cote:, :-cote] + cumul[:-cote, :-cote])


def nettoyer(mns, mnt):
    """Retire du MNS le mât et les câbles ; renvoie (mns nettoyé, description du
       mât ou None)."""
    surhauteur = mns - mnt

    # Le mât : quelques points à plus de 30 m au-dessus du sol nu. On note sa
    # position et sa hauteur pour le rebâtir, puis on le remplace par la médiane
    # de son voisinage, qui est le toit sur lequel il repose.
    mat = surhauteur > SEUIL_MAT
    releve = None
    if mat.any():
        rangees, colonnes = np.nonzero(mat)
        marge = 8
        r0, r1 = max(0, rangees.min() - marge), min(mns.shape[0], rangees.max() + marge + 1)
        c0, c1 = max(0, colonnes.min() - marge), min(mns.shape[1], colonnes.max() + marge + 1)
        voisinage = mns[r0:r1, c0:c1][~mat[r0:r1, c0:c1]]
        pied = float(np.median(voisinage)) if voisinage.size else float(mnt[mat].min())
        releve = {
            "rangee": float(rangees.mean()),
            "colonne": float(colonnes.mean()),
            "pied": pied,
            "sommet": float(mns[mat].max()),
            "points": int(mat.sum()),
        }
        mns = np.where(mat, pied, mns)

    # Les câbles du téléphérique : des points en l'air sans voisinage bâti.
    bati = (mns - mnt) > SEUIL_BATI
    isole = bati & (compte_voisins(bati, 5) < VOISINS_MIN)
    mns = np.where(isole, mnt, mns)
    return mns, releve, int(isole.sum())


def mediane_masquee(valeurs, masque, cote):
    """Médiane sur un carré cote x cote, calculée sur les seuls points marqués.
       Les points hors masque ne votent pas et gardent leur valeur."""
    rayon = cote // 2
    retenues = np.where(masque, valeurs, np.nan)
    pile = [np.roll(np.roll(retenues, dj, axis=0), di, axis=1)
            for dj in range(-rayon, rayon + 1)
            for di in range(-rayon, rayon + 1)]
    with warnings.catch_warnings():   # une maille sans aucun voisin marqué
        warnings.simplefilter("ignore", RuntimeWarning)
        mediane = np.nanmedian(np.stack(pile), axis=0)
    return np.where(np.isnan(mediane), valeurs, mediane)


def durcir_bati(mns, mnt):
    """Redresse les arêtes du bâti.

       Le MNS ne tranche pas net au bord d'un toit : il y descend sur un à deux
       mètres, si bien qu'un bâtiment de dix mètres arrive avec un biseau tout
       autour. Maillé tel quel, il ne ressemble plus à un bâtiment mais à un
       rocher fondu.

       On ramène donc chaque point bâti à la médiane de ses voisins bâtis. Les
       points de bordure, à mi-hauteur entre sol et toit, remontent au niveau du
       toit : la toiture retrouve son arête et le mur devient une marche franche
       d'une seule maille. Le rocher, lui, n'est pas touché, et une coupole non
       plus : la médiane suit une surface lisse sans la raboter."""
    bati = (mns - mnt) > SEUIL_BATI
    # Seul le BORD est redressé. Passer la médiane sur tout le bâti rabotait
    # aussi ce qui se dresse à l'intérieur d'un ensemble (mesuré : jusqu'à 11 m
    # de perte), coupoles comprises.
    interieur = bati.copy()
    for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        interieur &= np.roll(np.roll(bati, dj, axis=0), di, axis=1)
    bord = bati & ~interieur
    correction = np.clip(mediane_masquee(mns, bati, 5) - mns, -CORRECTION_MAX, CORRECTION_MAX)
    return np.where(bord, mns + correction, mns), bati


def _rayons_de_calotte(mns, j, i, pas):
    """Distances, dans huit directions, auxquelles la surface descend de
       CHUTE_COUPOLE sous le sommet : l'empreinte de la calotte."""
    nz, nx = mns.shape
    sommet = mns[j, i]
    distances = []
    for dj, di in ((0, 1), (0, -1), (1, 0), (-1, 0), (1, 1), (1, -1), (-1, 1), (-1, -1)):
        longueur = math.hypot(dj, di) * pas
        distance = 0.0
        for k in range(1, 16):
            jj, ii = j + dj * k, i + di * k
            if not (0 <= jj < nz and 0 <= ii < nx):
                break
            distance = k * longueur
            if sommet - mns[jj, ii] > CHUTE_COUPOLE:
                break
        distances.append(distance)
    return np.array(distances)


def rebatir_coupoles(mns, bati, pas):
    """Repère les coupoles et les remplace par de vraies demi-sphères.

       Repérage : un sommet local du bâti, dont la surface redescend à la même
       distance dans les huit directions. C'est la signature d'une calotte ronde,
       et elle écarte les toits plats comme les arêtes de bâtiment.

       Reconstruction : la demi-sphère s'appuie sur le toit qui entoure la
       calotte, avec pour rayon celui relevé. On prend le maximum de l'ancienne
       et de la nouvelle surface, pour ne jamais creuser ce que la donnée
       montre."""
    nz, nx = mns.shape
    # Sommet local sur 5x5, vectorisé.
    plus_haut = mns.copy()
    for dj in range(-2, 3):
        for di in range(-2, 3):
            plus_haut = np.maximum(plus_haut, np.roll(np.roll(mns, dj, axis=0), di, axis=1))
    candidats = bati & (mns >= plus_haut - 1e-6)

    grille_j, grille_i = np.mgrid[0:nz, 0:nx]
    retenues, refuses = [], 0
    for j, i in sorted(zip(*np.nonzero(candidats)), key=lambda p: -mns[p[0], p[1]]):
        if any(math.hypot(j - jj, i - ii) * pas < 6.0 for jj, ii, _, _ in retenues):
            continue
        rayons = _rayons_de_calotte(mns, j, i, pas)
        rayon = float(np.median(rayons))
        if not (RAYON_COUPOLE[0] <= rayon <= RAYON_COUPOLE[1]):
            continue
        if rayons.max() / max(rayons.min(), 1e-3) > REGULARITE_COUPOLE:
            refuses += 1
            continue
        # Niveau du toit sur lequel la coupole repose : l'anneau à son bord.
        distance = np.hypot((grille_j - j) * pas, (grille_i - i) * pas)
        anneau = (distance > rayon - pas) & (distance < rayon + pas) & bati
        assise = float(np.median(mns[anneau])) if anneau.any() else float(mns[j, i]) - rayon
        retenues.append((j, i, rayon, assise))

    masque = np.zeros_like(mns, dtype=bool)
    for j, i, rayon, assise in retenues:
        distance = np.hypot((grille_j - j) * pas, (grille_i - i) * pas)
        dedans = distance <= rayon
        demi_sphere = assise + np.sqrt(np.maximum(rayon * rayon - distance * distance, 0.0))
        mns = np.where(dedans, np.maximum(mns, demi_sphere), mns)
        masque |= dedans

    return mns, retenues, masque
