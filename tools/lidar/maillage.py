"""
maillage.py
Passage d'un champ de hauteurs à un maillage texturé, en repère moteur (X est,
Y haut, Z sud) : normales, coordonnées de texture, facettes franches sur le
bâti, et reconstruction des mâts.

Auteur : O. Booklage
Licence : GPL v2
"""

import math

import numpy as np

from lidar.carte import flou_boite

SEUIL_MUR = 3.0   # marche dans une maille de bâti : facette franche plutôt que lissée (m)

# Mât reconstruit : le pylône réel fait quelques mètres de côté ; plus épais, il
# tourne au tronc d'arbre dès qu'on s'en approche.
MAT_RAYON_BAS, MAT_RAYON_HAUT, MAT_COTES = 1.2, 0.35, 6

# Bande de façade (option --facade des outils). Essayée sur l'observatoire du Pic
# du Midi et écartée : les murs sont un escalier de mailles d'un mètre, chaque
# facette va chercher un morceau de photo différent et l'ensemble tourne au
# damier. Conservée pour des murs un jour modelés à la main.
BANDE_FACADE_PX = 640    # hauteur de la bande dans la texture
FACADE_PERIODE = 30.0    # mètres de mur couverts par une largeur de bande
FACADE_HAUTEUR = 7.5     # mètres de mur couverts par la hauteur de la bande


def couleur_de_facade(image, bati, nx, nz):
    """Teinte à donner aux murs : celle des toits, désaturée et éclaircie.

       Prendre la teinte des toits telle quelle passait à Dax, où les maisons
       sont séparées par des jardins, mais transformait Capbreton en magma rose :
       le front de mer y est fait de bâtiments contigus à tuile orange, et les
       murs prenaient la même couleur qu'eux. Un mur n'est pas un toit ; sous ces
       latitudes il est clair et neutre. On garde donc la clarté du lieu et un
       quart de sa teinte, pour ne pas plaquer un gris d'usine sur toutes les
       cartes."""
    pixels = np.asarray(image, dtype=np.float64)
    hauteur, largeur, _ = pixels.shape
    colonnes = (np.arange(nx) / (nx - 1) * (largeur - 1)).astype(int)
    rangees = (np.arange(nz) / (nz - 1) * (hauteur - 1)).astype(int)
    echantillon = pixels[np.ix_(rangees, colonnes)]
    if not bati.any():
        return np.array([180.0, 180.0, 178.0])
    toits = np.median(echantillon[bati], axis=0)
    moyenne = max(float(toits.mean()), 1.0)
    # Un mur est clair même sous un toit sombre. Suivre la clarté du toit de trop
    # près donnait, à La Mongie et ses ardoises, des murs à 136 de gris, donc de
    # la même teinte que les toitures : les bâtiments perdaient leurs arêtes et
    # tournaient au tas de sable. La droite ci-dessous relève les lieux sombres
    # sans blanchir ceux qui sont déjà clairs (126 -> 177, 172 -> 197, 203 -> 211).
    clarte = 120.0 + 0.45 * moyenne
    return np.clip(clarte * (0.85 + 0.15 * toits / moyenne), 0.0, 255.0)


def uv_de_facade(image, bati, nx, nz, facteur_v=1.0):
    """Point de texture qui teintera toutes les façades du bâti : celui, parmi
       les toits, dont la couleur est leur médiane.

       Un placage vertical n'a rien à donner à un mur. Lui laisser la couleur de
       l'arête de toit maille par maille, comme une première version le faisait,
       cannelle les bâtiments de bandes verticales, chacune tirée d'un pixel
       différent, souvent un mélange de toit et de sol : l'ensemble prend
       l'aspect d'un rocher. Une teinte unique, prise sur les toits eux-mêmes
       (mesuré ici : luminance 180 contre 136 pour le rocher), rend au bâti sa
       planéité et le détache du relief."""
    pixels = np.asarray(image, dtype=np.float64)
    hauteur, largeur, _ = pixels.shape
    colonnes = (np.arange(nx) / (nx - 1) * (largeur - 1)).astype(int)
    rangees = (np.arange(nz) / (nz - 1) * (hauteur - 1)).astype(int)
    echantillon = pixels[np.ix_(rangees, colonnes)]
    if not bati.any():
        return None
    mediane = np.median(echantillon[bati], axis=0)
    ecart = np.linalg.norm(echantillon - mediane, axis=2)
    ecart[~bati] = np.inf
    j, i = np.unravel_index(np.argmin(ecart), ecart.shape)
    return [i / (nx - 1), j / (nz - 1) * facteur_v]   # V vers le bas, voir construire_maillage


def normales_lissees(hauteurs, pas):
    """Normales du champ de hauteurs, par gradient central : lisses, donc justes
       sur les coupoles et les pentes."""
    dz, dx = np.gradient(hauteurs, pas)   # axe 0 = sud, axe 1 = est
    normales = np.stack([-dx, np.ones_like(hauteurs), -dz], axis=-1)
    return normales / np.linalg.norm(normales, axis=-1, keepdims=True)


def construire_maillage(hauteurs, pas, demi_x, demi_z, bati=None, uv_facade=None,
                        bande=None, facteur_v=1.0, coupoles=None):
    """Maillage régulier du champ de hauteurs, en repère moteur (X est, Y haut,
       Z sud), texturé par placage vertical.

       Les mailles franchissant une marche de plus de SEUIL_MUR reçoivent la
       normale de leur facette au lieu de la normale lissée : les façades
       restent nettes là où le lissage les ferait fondre, sans toucher aux
       coupoles."""
    nz, nx = hauteurs.shape
    x = -demi_x + np.arange(nx) * pas
    z = -demi_z + np.arange(nz) * pas
    positions = np.stack([np.repeat(x[None, :], nz, 0),
                          hauteurs,
                          np.repeat(z[:, None], nx, 1)], axis=-1).reshape(-1, 3)
    normales = normales_lissees(hauteurs, pas).reshape(-1, 3)
    # V compté vers le BAS de l'image, convention du glTF, contraire à celle des
    # .ac de Paris. Mesuré, et non déduit : une marque posée dans la texture au
    # pied du mât ressortait 130 m au sud du mât tant que V montait. Le rocher
    # recevait alors la photo des toits, et les bâtiments celle du rocher, d'où
    # leur aspect de cailloux.
    u = np.repeat((np.arange(nx) / (nx - 1))[None, :], nz, 0)
    # facteur_v ramène l'orthophoto à sa part de l'image quand une bande de
    # façade est collée dessous.
    v = np.repeat((np.arange(nz) / (nz - 1) * facteur_v)[:, None], nx, 1)
    uvs = np.stack([u, v], axis=-1).reshape(-1, 2)

    # Indices des quatre coins de chaque maille (rangée j = nord -> sud).
    coin = np.arange(nz * nx).reshape(nz, nx)
    v00, v10 = coin[:-1, :-1], coin[:-1, 1:]
    v01, v11 = coin[1:, :-1], coin[1:, 1:]

    # Une maille est un mur si elle touche du bâti ET que ses quatre altitudes
    # s'écartent de plus du seuil. Le rocher, lui, reste lissé quelle que soit sa
    # pente : une falaise est une surface raide, pas un empilement de facettes.
    quatre = np.stack([hauteurs[:-1, :-1], hauteurs[:-1, 1:],
                       hauteurs[1:, :-1], hauteurs[1:, 1:]])
    mur = (quatre.max(axis=0) - quatre.min(axis=0)) > SEUIL_MUR

    # Normale des murs : direction de plus grande pente du relief ADOUCI, prise
    # à l'horizontale, et non la normale de chaque facette. Un mur suit l'emprise
    # du bâtiment en escalier de mailles : facette par facette, la normale bascule
    # d'un pan à l'autre à chaque marche, et le mur se raye de bandes claires et
    # sombres alternées. Lissée, elle regarde vers l'extérieur du bâtiment tout du
    # long, et le mur s'éclaire d'un seul tenant.
    adouci = flou_boite(hauteurs, 5)
    pente_z, pente_x = np.gradient(adouci, pas)
    px = 0.25 * (pente_x[:-1, :-1] + pente_x[:-1, 1:] + pente_x[1:, :-1] + pente_x[1:, 1:])
    pz = 0.25 * (pente_z[:-1, :-1] + pente_z[:-1, 1:] + pente_z[1:, :-1] + pente_z[1:, 1:])
    normale_mur = np.stack([-px, np.zeros_like(px), -pz], axis=-1)
    longueurs = np.linalg.norm(normale_mur, axis=-1, keepdims=True)
    normale_mur = np.divide(normale_mur, longueurs, out=np.zeros_like(normale_mur),
                            where=longueurs > 1e-6)
    if bati is not None:
        touche = (bati[:-1, :-1] | bati[:-1, 1:] | bati[1:, :-1] | bati[1:, 1:])
        mur &= touche
    if coupoles is not None:
        # Une coupole est une surface courbe, pas un mur : ni facette franche,
        # ni rangées de fenêtres sur son flanc.
        mur &= ~(coupoles[:-1, :-1] | coupoles[:-1, 1:] | coupoles[1:, :-1] | coupoles[1:, 1:])

    # Mailles lisses : deux triangles sur les sommets partagés. L'ordre des
    # sommets donne une normale vers le haut (X est, Y haut, Z sud).
    lisse = ~mur
    indices = np.concatenate([
        np.stack([v00[lisse], v01[lisse], v10[lisse]], axis=-1).reshape(-1),
        np.stack([v10[lisse], v01[lisse], v11[lisse]], axis=-1).reshape(-1),
    ])

    # Mailles à marche : sommets propres, normale de facette.
    #
    # Sur le bâti, la façade prend en plus une teinte unique, celle du toit
    # médian (voir uv_de_facade). Le rocher, lui, garde sa photo.
    supplement_pos, supplement_uv, supplement_nrm, supplement_idx = [], [], [], []
    base = positions.shape[0]
    for (j, i), (a, b, c) in _triangles_de_mur(v00, v01, v10, v11, mur):
        trio = positions[[a, b, c]]
        normale = normale_mur[j, i]
        if not normale.any():   # replat sans direction : normale de la facette
            normale = np.cross(trio[1] - trio[0], trio[2] - trio[0])
            longueur = np.linalg.norm(normale)
            normale = normale / longueur if longueur > 1e-9 else np.array([0.0, 1.0, 0.0])
        if bande is not None and bati is not None and bati[j:j + 2, i:i + 2].any():
            # Placage sur la bande de façade : U court le long du mur (la
            # tangente à sa normale), V descend depuis la ligne de toit. Deux
            # mailles voisines d'un même toit partagent donc leurs rangées de
            # fenêtres.
            # Tangente ramenée à l'axe du monde le plus proche. Prise telle
            # quelle, elle tourne d'une maille à l'autre le long d'un mur en
            # escalier, et chaque maille va chercher un morceau de bande
            # différent : les fenêtres partent en damier.
            tangente = (np.array([0.0, 0.0, 1.0]) if abs(normale[0]) > abs(normale[2])
                        else np.array([1.0, 0.0, 0.0]))
            toit = trio[:, 1].max()
            coordonnees = np.empty((3, 2))
            for k in range(3):
                coordonnees[k, 0] = (trio[k, 0] * tangente[0]
                                     + trio[k, 2] * tangente[2]) / FACADE_PERIODE
                descente = min(max((toit - trio[k, 1]) / FACADE_HAUTEUR, 0.0), 1.0)
                coordonnees[k, 1] = bande[0] + descente * (bande[1] - bande[0])
        elif uv_facade is not None and bati is not None and bati[j:j + 2, i:i + 2].any():
            coordonnees = np.tile(np.array(uv_facade), (3, 1))
        else:
            coordonnees = uvs[[a, b, c]]
        supplement_pos.append(trio)
        supplement_uv.append(coordonnees)
        supplement_nrm.append(np.tile(normale, (3, 1)))
        supplement_idx.append([base, base + 1, base + 2])
        base += 3

    if supplement_pos:
        positions = np.concatenate([positions, np.concatenate(supplement_pos)])
        normales = np.concatenate([normales, np.concatenate(supplement_nrm)])
        uvs = np.concatenate([uvs, np.concatenate(supplement_uv)])
        indices = np.concatenate([indices, np.array(supplement_idx).reshape(-1)])

    return positions, normales, uvs, indices.astype(np.uint32), int(mur.sum())


def _triangles_de_mur(v00, v01, v10, v11, mur):
    """Les deux triangles de chaque maille marquée comme mur, avec sa position
       dans la grille."""
    for j, i in zip(*np.nonzero(mur)):
        yield (j, i), (int(v00[j, i]), int(v01[j, i]), int(v10[j, i]))
        yield (j, i), (int(v10[j, i]), int(v01[j, i]), int(v11[j, i]))


def geometrie_mat(colonne, rangee, bas, haut, pas, demi_x, demi_z, uv_reference):
    """Le mât de l'antenne, en tronc de cône à six pans. Écrêté du MNS parce
       qu'il n'y tient qu'en une aiguille de deux points, il est reconstruit
       ici : c'est la silhouette qu'on reconnaît à des kilomètres."""
    cx = -demi_x + colonne * pas
    cz = -demi_z + rangee * pas

    positions, normales, uvs, indices = [], [], [], []
    for k in range(MAT_COTES):
        a0 = 2.0 * math.pi * k / MAT_COTES
        a1 = 2.0 * math.pi * (k + 1) / MAT_COTES
        p = [
            [cx + MAT_RAYON_BAS * math.cos(a0), bas, cz + MAT_RAYON_BAS * math.sin(a0)],
            [cx + MAT_RAYON_BAS * math.cos(a1), bas, cz + MAT_RAYON_BAS * math.sin(a1)],
            [cx + MAT_RAYON_HAUT * math.cos(a1), haut, cz + MAT_RAYON_HAUT * math.sin(a1)],
            [cx + MAT_RAYON_HAUT * math.cos(a0), haut, cz + MAT_RAYON_HAUT * math.sin(a0)],
        ]
        milieu = 0.5 * (a0 + a1)
        normale = [math.cos(milieu), 0.0, math.sin(milieu)]
        depart = len(positions)
        positions.extend(p)
        normales.extend([normale] * 4)
        uvs.extend([uv_reference] * 4)
        indices.extend([depart, depart + 1, depart + 2, depart, depart + 2, depart + 3])

    return (np.array(positions), np.array(normales), np.array(uvs),
            np.array(indices, dtype=np.uint32))
