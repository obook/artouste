#!/usr/bin/env python3
"""
estacade.py
Fabrique les deux môles du chenal du Boucarot à Capbreton : la jetée sud, qui
porte l'estacade et les feux verts, et la jetée nord, qui porte le feu rouge.
Enrochement surmonté d'une promenade et d'un muret dans les deux cas.

Pourquoi un modèle et pas une pièce de surface LiDAR : le laser aéroporté ne
résout pas les blocs d'enrochement, il les moyenne. La pièce tirée du MNS rendait
un monticule lisse, et l'emprise rectangulaire posée sur la mer y ajoutait un
plateau. Résultat, un tas de sable.

La section vient du môle générique de FlightGear (Models/Maritime/Misc/
pier_200m.ac) : base large, talus, promenade, muret côté mer. Elle est reprise
aux cotes de Capbreton, mesurées au LiDAR HD : crête à 2,7 m au 95e centile,
enrochement d'une trentaine de mètres de large. La texture est prélevée sur les
vrais blocs dans la BD ORTHO ; celle de FlightGear a été essayée d'abord et
écartée, c'est un grès beige qui rendait le môle plus sableux que la photo.

L'axe est celui de la BD TOPO (tronçon "Sentier" de la jetée), dont les
altitudes sont vides au-dessus de l'eau mais dont le tracé en plan est bon.

Usage : python3 tools/monuments/estacade.py
Sortie : assets/models/monuments/capbreton/estacade.ac (+ la texture à côté)

Auteur : O. Booklage
Licence : GPL v2
"""

import math
import sys
from pathlib import Path

from PIL import Image

RACINE = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(RACINE / "tools"))
from lidar.services import telecharger_ortho
SORTIE = RACINE / "assets" / "models" / "monuments" / "capbreton"
TEXTURE = "enrochement.png"

# Axe de la jetée, relevé BD TOPO (tronçon de 487 m, ses quatre derniers points :
# le reste est à terre). Du rivage vers le large.
AXE_SUD = [
    (-1.445246, 43.654991),
    (-1.445682, 43.655078),
    (-1.446559, 43.655244),
    (-1.447463, 43.655419),
    # La BD TOPO s'arrête où finit le cheminement, pas où finit l'ouvrage : la
    # tête d'enrochement continue 40 m au large, mesurés sur la BD ORTHO à
    # 0,20 m/px, dernier bloc à (-1.447942, 43.655512). Sans ce point le modèle
    # laissait le bout de la jetée à découvert.
    (-1.447942, 43.655512),
]

# Jetée nord, du large vers le rivage. Le tronçon "Sentier" de la BD TOPO suit
# le BORD du chenal et non le haut de la digue : le milieu du plateau s'en écarte
# de 8 à 20 m. L'axe ci-dessous est celui du plateau, relevé au MNS LiDAR par
# coupes tous les 23 m, en prenant le milieu de ce qui dépasse 2,2 m.
# Elle s'arrête à 82 m : au-delà l'orthophoto ne montre que du sable, la digue
# ayant rejoint la plage. Le relevé de plateau, lui, continuait à 3,5 m, mais
# c'était la dune et non l'ouvrage.
AXE_NORD = [
    (-1.447287, 43.655986),
    (-1.447432, 43.655977),
    (-1.447711, 43.656002),
    (-1.447997, 43.656002),
    (-1.448296, 43.655952),
    # Prolongé de 12 m au large : la balise rouge, relevée sur photo à
    # (-1.448380, 43.655947), tombait au-delà du bout du relevé de plateau.
    (-1.448439, 43.655928),
]

# Section en travers, en mètres : x positif vers le large (côté océan), y vers le
# haut, zéro au niveau de la mer. Le contour se lit du pied côté océan au pied
# côté chenal.
SECTION = [
    (13.0, 0.0),    # pied de l'enrochement, côté océan
    (4.5, 2.6),     # talus
    (3.6, 4.6),     # muret pare-lame
    (2.6, 4.6),
    (2.6, 3.4),     # bord de la promenade
    (-3.5, 3.4),    # promenade
    (-4.5, 2.6),    # talus côté chenal
    (-11.0, 0.0),   # pied de l'enrochement, côté chenal
]

# Taille au sol d'une tuile de texture, en mètres. La texture est un carré de
# 24 m d'enrochement prélevé dans la BD ORTHO à 0,20 m/px, replié en miroir pour
# se raccorder : 48 m de côté. La pierre de FlightGear a été essayée d'abord et
# écartée, c'est un grès beige qui rendait le môle plus sableux que la photo.
TUILE_U = 48.0
TUILE_V = 24.0

# Longueur du nez, en mètres. Une tête d'enrochement plonge en pente dans l'eau ;
# fermée par une face verticale, elle avait l'air tranchée au couteau. La section
# est donc réduite sur les derniers mètres, en racine carrée pour que le nez
# s'arrondisse au lieu de filer en pointe. Le dernier point de chaque axe est
# celui du large : les deux listes vont du rivage vers la mer.
# Le nez est plus court au nord : le MNS y donne la crête à 3,6 m jusqu'au bout,
# la balise rouge se tenant à 6,8 m de l'extrémité. Un nez de 14 m l'aurait
# laissée sur une section réduite de moitié, donc dans le vide.
NEZ_SUD_M = 14.0
NEZ_NORD_M = 6.0
NEZ_MIN = 0.12

# Prélèvement de la texture : un carré d'enrochement de la jetée, en pleine
# BD ORTHO. Replié en miroir pour que la tuile se raccorde à elle-même.
TEXTURE_LON, TEXTURE_LAT = -1.44660, 43.65528
TEXTURE_DEMI_M = 12.0
TEXTURE_M_PAR_PX = 0.20

M_PAR_DEG_LAT = 111320.0


def fabriquer_texture(chemin):
    """Prélève l'enrochement dans la BD ORTHO et en fait une tuile raccordable."""
    d_lat = TEXTURE_DEMI_M / M_PAR_DEG_LAT
    d_lon = TEXTURE_DEMI_M / (M_PAR_DEG_LAT * math.cos(math.radians(TEXTURE_LAT)))
    cote = int(round(2 * TEXTURE_DEMI_M / TEXTURE_M_PAR_PX))
    carre = telecharger_ortho((TEXTURE_LAT - d_lat, TEXTURE_LON - d_lon,
                               TEXTURE_LAT + d_lat, TEXTURE_LON + d_lon), cote, cote)
    tuile = Image.new("RGB", (2 * cote, 2 * cote))
    tuile.paste(carre, (0, 0))
    tuile.paste(carre.transpose(Image.FLIP_LEFT_RIGHT), (cote, 0))
    tuile.paste(carre.transpose(Image.FLIP_TOP_BOTTOM), (0, cote))
    tuile.paste(carre.transpose(Image.ROTATE_180), (cote, cote))
    tuile.save(chemin)
    return tuile.size


def metres(axe):
    """Axe en mètres, X est et Z sud, centré sur son propre milieu."""
    lat_moy = sum(p[1] for p in axe) / len(axe)
    m_par_deg_lon = M_PAR_DEG_LAT * math.cos(math.radians(lat_moy))
    lon_moy = sum(p[0] for p in axe) / len(axe)
    return [((lon - lon_moy) * m_par_deg_lon, (lat_moy - lat) * M_PAR_DEG_LAT)
            for lon, lat in axe], (lon_moy, lat_moy)


def normales_de_section(points):
    """Direction perpendiculaire à l'axe en chaque point, moyennée aux sommets
       intérieurs pour que le ruban ne s'ouvre pas dans les virages."""
    n = len(points)
    sorties = []
    for i in range(n):
        dx = dz = 0.0
        for a, b in ((i - 1, i), (i, i + 1)):
            if 0 <= a and b < n:
                ex, ez = points[b][0] - points[a][0], points[b][1] - points[a][1]
                longueur = math.hypot(ex, ez)
                if longueur > 1e-6:
                    dx += ex / longueur
                    dz += ez / longueur
        longueur = math.hypot(dx, dz)
        sorties.append((dz / longueur, -dx / longueur))
    return sorties


def densifier_nez(axe_m, nez_m):
    """Ajoute des stations sur les derniers mètres. Sans elles le nez s'étirerait
       sur tout le dernier segment de l'axe, long de plusieurs dizaines de
       mètres, au lieu des NEZ_M voulus."""
    if len(axe_m) < 2:
        return axe_m
    ax, az = axe_m[-1][0] - axe_m[-2][0], axe_m[-1][1] - axe_m[-2][1]
    longueur = math.hypot(ax, az)
    if longueur <= nez_m:
        return axe_m
    sorties = list(axe_m[:-1])
    for reste in (nez_m, nez_m * 0.6, nez_m * 0.3, nez_m * 0.1, 0.0):
        t = 1.0 - reste / longueur
        sorties.append((axe_m[-2][0] + ax * t, axe_m[-2][1] + az * t))
    return sorties


def construire(axe_m, nez_m):
    """Sommets, UV et quadrilatères du balayage de SECTION le long de l'axe."""
    axe_m = densifier_nez(axe_m, nez_m)
    perp = normales_de_section(axe_m)
    parcouru = [0.0]
    for i in range(1, len(axe_m)):
        parcouru.append(parcouru[-1] + math.hypot(axe_m[i][0] - axe_m[i - 1][0],
                                                  axe_m[i][1] - axe_m[i - 1][1]))

    """V suit le CONTOUR de la section et non l'altitude : sur un talus, une
       hauteur de 2,6 m se développe sur 9 m de pente, et mapper V sur la seule
       altitude étirait les blocs d'un facteur trois le long du talus."""
    contour = [0.0]
    for k in range(1, len(SECTION)):
        contour.append(contour[-1] + math.hypot(SECTION[k][0] - SECTION[k - 1][0],
                                                SECTION[k][1] - SECTION[k - 1][1]))

    sommets, uv = [], []
    for i, (x, z) in enumerate(axe_m):
        px, pz = perp[i]
        reste = parcouru[-1] - parcouru[i]
        nez = 1.0 if reste >= nez_m else max(NEZ_MIN, math.sqrt(reste / nez_m))
        for k, (ecart, hauteur) in enumerate(SECTION):
            sommets.append((x + px * ecart * nez, hauteur * nez, z + pz * ecart * nez))
            uv.append((parcouru[i] / TUILE_U, contour[k] / TUILE_V))

    large = len(SECTION)
    faces = []
    for i in range(len(axe_m) - 1):
        for j in range(large - 1):
            a = i * large + j
            faces.append((a, a + 1, a + large + 1, a + large))
    # Capots : la section fermée à chaque bout, sinon on voit dans le môle.
    faces.append(tuple(range(large - 1, -1, -1)))
    debut = (len(axe_m) - 1) * large
    faces.append(tuple(range(debut, debut + large)))
    return sommets, uv, faces


def ecrire_ac(chemin, sommets, uv, faces):
    with open(chemin, "w", encoding="utf-8") as f:
        f.write("AC3Db\n")
        f.write("MATERIAL \"pierre\" rgb 0.72 0.72 0.72 amb 0.8 0.8 0.8 emis 0 0 0 "
                "spec 0.2 0.2 0.2 shi 32 trans 0\n")
        f.write("OBJECT world\nkids 1\n")
        f.write("OBJECT poly\nname \"estacade\"\n")
        f.write(f"texture \"{TEXTURE}\"\n")
        f.write(f"numvert {len(sommets)}\n")
        for x, y, z in sommets:
            f.write(f"{x:.4f} {y:.4f} {z:.4f}\n")
        f.write(f"numsurf {len(faces)}\n")
        for face in faces:
            f.write("SURF 0x30\nmat 0\n")
            f.write(f"refs {len(face)}\n")
            for k in face:
                f.write(f"{k} {uv[k][0]:.4f} {uv[k][1]:.4f}\n")
        f.write("kids 0\n")


def fabriquer(nom, axe, nez_m):
    axe_m, centre = metres(axe)
    longueur = sum(math.hypot(axe_m[i + 1][0] - axe_m[i][0], axe_m[i + 1][1] - axe_m[i][1])
                   for i in range(len(axe_m) - 1))
    sommets, uv, faces = construire(axe_m, nez_m)

    """Le moteur recentre tout monument sur sa boîte englobante en plan avant de
       le poser (ApplicationMonuments.cpp) : la coordonnée de monuments.txt
       désigne donc ce centre-là, pas le milieu de l'axe. Les deux diffèrent
       d'une dizaine de mètres, la section étant plus large côté océan. On cale
       donc la géométrie sur sa propre boîte, et le point déclaré tombe juste."""
    ecart_x = 0.5 * (min(x for x, _, _ in sommets) + max(x for x, _, _ in sommets))
    ecart_z = 0.5 * (min(z for _, _, z in sommets) + max(z for _, _, z in sommets))
    sommets = [(x - ecart_x, y, z - ecart_z) for x, y, z in sommets]
    m_par_deg_lon = M_PAR_DEG_LAT * math.cos(math.radians(centre[1]))
    centre = (centre[0] + ecart_x / m_par_deg_lon, centre[1] - ecart_z / M_PAR_DEG_LAT)

    chemin = SORTIE / f"{nom}.ac"
    ecrire_ac(chemin, sommets, uv, faces)
    largeur = max(e for e, _ in SECTION) - min(e for e, _ in SECTION)
    print(f"[mole] {chemin.name} : {len(sommets)} sommets, {len(faces)} faces, "
          f"{longueur:.0f} m de long, {largeur:.0f} m de large")
    print(f"    {centre[0]:.6f} {centre[1]:.6f} 0.0 0 1 1 {longueur / 2 + largeur:.0f} "
          f"capbreton/{nom}.ac")


def main():
    SORTIE.mkdir(parents=True, exist_ok=True)
    taille = fabriquer_texture(SORTIE / TEXTURE)
    print(f"[mole] {TEXTURE} : {taille[0]}x{taille[1]}, "
          f"{2 * TEXTURE_DEMI_M * 2:.0f} m de côté au sol")
    fabriquer("estacade", AXE_SUD, NEZ_SUD_M)
    fabriquer("jetee-nord", AXE_NORD, NEZ_NORD_M)


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        print(f"[erreur] {err}", file=sys.stderr)
        sys.exit(1)
