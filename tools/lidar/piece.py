"""
piece.py
Fabrication d'une PIÈCE DE SURFACE : un morceau de terrain réel, relevé au
laser, posé sur une carte du simulateur comme un monument.

C'est le coeur commun de tous les modèles tirés du LiDAR, quel que soit le sujet.
L'observatoire du Pic du Midi n'en est que le premier cas ; un aérodrome, un
village ou une zone de poser passent par le même chemin.

Le moteur ne sait pas afficher un morceau de terrain plus fin que sa carte : on
en fait donc un monument, mécanisme qui existe déjà et qui coûte un seul appel de
dessin. La pièce efface sous elle les bâtiments extrudés (rayon de dégagement de
monuments.txt) et, si besoin, les arbres (exclusions.txt de la carte).

Auteur : O. Booklage
Licence : GPL v2
"""

import math
from pathlib import Path

import numpy as np
from PIL import Image

from terrain import config
from lidar.services import (COUCHE_MNS, COUCHE_MNT, telecharger_ortho,
                            telecharger_relief)
from lidar.carte import (EPAISSEUR_SOL, M_PAR_DEG_LAT, fondu_des_bords,
                         recaler_sur_la_carte, sol_de_la_carte)
from lidar.bati import SEUIL_BATI, durcir_bati, nettoyer, rebatir_coupoles
from lidar.maillage import (BANDE_FACADE_PX, FACADE_HAUTEUR, FACADE_PERIODE,
                            construire_maillage, couleur_de_facade, geometrie_mat,
                            uv_de_facade)
from lidar.gltf import ecrire_glb, relire_glb


def fabriquer(args):
    dlat = 1.0 / M_PAR_DEG_LAT
    dlon = 1.0 / (M_PAR_DEG_LAT * math.cos(math.radians(args.lat)))

    nx = int(round(2.0 * args.demi_x / args.pas)) + 1
    nz = int(round(2.0 * args.demi_z / args.pas)) + 1

    # Les points de grille sont des NOEUDS, le premier sur le bord ouest. Le
    # service rend des PIXELS, dont les centres tombent à un demi-pas à
    # l'intérieur : on élargit d'un demi-pas pour que les deux coïncident.
    demi = 0.5 * args.pas
    bbox_relief = (args.lat - (args.demi_z + demi) * dlat,
                   args.lon - (args.demi_x + demi) * dlon,
                   args.lat + (args.demi_z + demi) * dlat,
                   args.lon + (args.demi_x + demi) * dlon)
    bbox_ortho = (args.lat - args.demi_z * dlat, args.lon - args.demi_x * dlon,
                  args.lat + args.demi_z * dlat, args.lon + args.demi_x * dlon)

    print(f"[emprise] {2 * args.demi_x:.0f} x {2 * args.demi_z:.0f} m autour de "
          f"{args.lon:.5f} {args.lat:.5f}, pas {args.pas} m ({nx} x {nz} points)")

    print("[relief] MNT et MNS LiDAR HD...")
    mnt = telecharger_relief(COUCHE_MNT, bbox_relief, nx, nz)
    mns = telecharger_relief(COUCHE_MNS, bbox_relief, nx, nz)
    if (mnt <= config.NODATA).any() or (mns <= config.NODATA).any():
        raise RuntimeError("le LiDAR HD ne couvre pas toute l'emprise demandée")

    mns, mat, cables = nettoyer(mns, mnt)
    print(f"[nettoyage] {cables} points isolés retirés (câbles, lignes)")
    if mat:
        print(f"[nettoyage] mât écrêté : {mat['points']} points, "
              f"{mat['sommet'] - mat['pied']:.1f} m au-dessus de son pied")
    else:
        print("[nettoyage] aucun mât trouvé au-dessus du seuil")

    # Sol de la carte aux mêmes points, pour le raccord des bords.
    lignes = np.arange(nz)[:, None]
    colonnes = np.arange(nx)[None, :]
    lons = args.lon + (-args.demi_x + colonnes * args.pas) * dlon
    lats = args.lat - (-args.demi_z + lignes * args.pas) * dlat
    sol = sol_de_la_carte(args.carte, np.broadcast_to(lons, (nz, nx)),
                          np.broadcast_to(lats, (nz, nx)))

    mns, bati = durcir_bati(mns, mnt)
    print(f"[bâti] {int(bati.sum())} points redressés, hauteur médiane "
          f"{np.median((mns - mnt)[bati]):.1f} m")

    masque_coupoles = None
    if args.coupoles:
        mns, coupoles, masque_coupoles = rebatir_coupoles(mns, bati, args.pas)
        print(f"[coupoles] {len(coupoles)} rebâties en demi-sphères :")
        for j, i, rayon, assise in coupoles:
            print(f"[coupoles]   maille ({i},{j}) : rayon {rayon:.1f} m, "
                  f"assise {assise:.1f} m, sommet {assise + rayon:.1f} m")

    mns_recale, ecart = recaler_sur_la_carte(mns, mnt, sol)
    print(f"[recalage] carte moins LiDAR : médiane {np.median(sol - mnt):+.1f} m, "
          f"correction de {ecart.min():+.1f} à {ecart.max():+.1f} m ; "
          f"reste hors bâti {np.median((mns_recale - sol)[~bati]):+.1f} m")
    # Le modèle ne passe jamais sous le relief de la carte, sinon le maillage du
    # terrain, dix-sept fois plus lâche, ressortirait au travers.
    hauteurs = np.maximum(mns_recale, sol + EPAISSEUR_SOL)
    poids = fondu_des_bords(nx, nz, args.pas, args.marge)
    hauteurs = poids * hauteurs + (1.0 - poids) * sol

    print("[texture] BD ORTHO à %.2f m/px..." % args.ortho_m_par_px)
    largeur = int(round(2 * args.demi_x / args.ortho_m_par_px))
    hauteur = int(round(2 * args.demi_z / args.ortho_m_par_px))
    if max(largeur, hauteur) > config.WMS_MAX_PX:
        raise RuntimeError(f"texture {largeur}x{hauteur} au-dessus de la limite "
                           f"du service ({config.WMS_MAX_PX} px)")
    args.sortie.mkdir(parents=True, exist_ok=True)
    ortho = telecharger_ortho(bbox_ortho, largeur, hauteur)

    # Bande de façade collée SOUS l'orthophoto, dans la même image : un seul
    # matériau, une seule texture, et des murs qui cessent d'être des aplats.
    image, bande, facteur_v = ortho, None, 1.0
    if args.facade:
        totale = hauteur + BANDE_FACADE_PX
        image = Image.new("RGB", (largeur, totale))
        image.paste(ortho, (0, 0))
        image.paste(Image.open(args.facade).convert("RGB").resize((largeur, BANDE_FACADE_PX)),
                    (0, hauteur))
        facteur_v = (hauteur - 1) / (totale - 1)
        bande = (hauteur / (totale - 1), 1.0)
        print(f"[façade] {args.facade} : bande {largeur}x{BANDE_FACADE_PX} px, "
              f"{FACADE_PERIODE:.0f} m de mur par largeur, {FACADE_HAUTEUR:.0f} m par hauteur")

    # Bandeau de teinte des murs, collé sous l'orthophoto : deux rangées d'une
    # seule couleur, celle que couleur_de_facade a calculée. Le placage vertical
    # n'ayant rien à dire d'un mur, il vaut mieux lui donner une teinte choisie
    # qu'un pixel de toit pris au hasard.
    if bande is None:
        teinte = couleur_de_facade(ortho, bati, nx, nz)
        totale = hauteur + 8
        agrandie = Image.new("RGB", (largeur, totale), tuple(int(c) for c in teinte))
        agrandie.paste(image, (0, 0))
        image = agrandie
        facteur_v = (hauteur - 1) / (totale - 1)
        uv_facade = [0.5, (hauteur + 4) / (totale - 1)]
        print(f"[façade] murs en teinte unie {tuple(int(c) for c in teinte)}, "
              f"tirée des toits du lieu")
    else:
        uv_facade = uv_de_facade(ortho, bati, nx, nz, facteur_v)

    chemin_image = args.sortie / f"{args.nom}.jpg"
    image.save(chemin_image, quality=config.ORTHO_JPEG_QUALITY)
    positions, normales, uvs, indices, murs = construire_maillage(
        hauteurs, args.pas, args.demi_x, args.demi_z, bati, uv_facade, bande, facteur_v,
        masque_coupoles)
    print(f"[maillage] {murs} mailles à marche traitées en facettes franches")

    if mat:
        # Le mât hérite d'un point de texture pris sur son pied : une seule
        # teinte, celle du toit qui le porte. Il se dresse depuis le modèle
        # rendu, non depuis l'altitude absolue du MNS, puisque le modèle est une
        # surhauteur posée sur le relief de la carte.
        colonne = int(round(mat["colonne"]))
        rangee = int(round(mat["rangee"]))
        uv_mat = [colonne / (nx - 1), rangee / (nz - 1) * facteur_v]
        pied = float(hauteurs[rangee, colonne])
        p, n, t, idx = geometrie_mat(colonne, rangee, pied,
                                     pied + (mat["sommet"] - mat["pied"]),
                                     args.pas, args.demi_x, args.demi_z, uv_mat)
        indices = np.concatenate([indices, idx + len(positions)])
        positions = np.concatenate([positions, p])
        normales = np.concatenate([normales, n])
        uvs = np.concatenate([uvs, t])

    chemin_glb = args.sortie / f"{args.nom}.glb"
    ecrire_glb(chemin_glb, positions, normales, uvs, indices, chemin_image.name, args.nom)

    triangles = len(indices) // 3
    print(f"[modèle] {chemin_glb} : {triangles} triangles, {len(positions)} sommets, "
          f"{chemin_glb.stat().st_size / 1e6:.1f} Mo")
    print(f"[modèle] {chemin_image} : {image.size[0]}x{image.size[1]}, "
          f"{chemin_image.stat().st_size / 1e6:.1f} Mo")
    print(f"[modèle] altitudes {positions[:, 1].min():.1f} à {positions[:, 1].max():.1f} m")

    # Le moteur pose le POINT LE PLUS BAS du modèle à l'altitude déclarée ; le
    # mot-clé "sol" ne conviendrait pas, le relief de la carte sous le centre
    # étant bien plus haut que le bord du modèle.
    print("\n[monuments.txt] ligne à écrire dans "
          f"assets/terrain/{args.carte}/monuments.txt :\n")
    # Rayon de dégagement : le cercle qui CONTIENT la pièce. Le moteur ne sait
    # dégager qu'un disque ; le prendre inscrit laisserait des bâtiments
    # extrudés traverser la surface aux extrémités, ce qui se voit bien plus
    # qu'un bâtiment manquant dans l'anneau autour.
    rayon = math.hypot(args.demi_x, args.demi_z)
    print(f"{args.lon:.5f} {args.lat:.5f} {positions[:, 1].min():.1f} 0 1 1 {rayon:.0f} "
          f"{args.sortie.name}/{args.nom}.glb {args.titre}\n")
    return float(positions[:, 1].min())


def verifier(args):
    """Relit ce qui a été écrit et vérifie ce qui doit l'être : la géométrie
       tient dans l'emprise annoncée, la texture est là, le compte de triangles
       reste sous le budget. Échoue bruyamment si le fichier a dérivé."""
    chemin_glb = args.sortie / f"{args.nom}.glb"
    chemin_image = args.sortie / f"{args.nom}.jpg"
    scene, longueur_bin = relire_glb(chemin_glb)

    accesseurs = scene["accessors"]
    sommets, triangles = accesseurs[0]["count"], accesseurs[3]["count"] // 3
    bas, haut = accesseurs[0]["min"], accesseurs[0]["max"]

    assert scene["buffers"][0]["byteLength"] <= longueur_bin, "bloc binaire trop court"
    assert scene["images"][0]["uri"] == chemin_image.name, "texture non référencée"
    assert chemin_image.exists(), f"{chemin_image} manquant"
    assert accesseurs[1]["count"] == sommets, "autant de normales que de sommets"
    assert accesseurs[2]["count"] == sommets, "autant d'UV que de sommets"
    assert accesseurs[3]["count"] % 3 == 0, "les indices vont par trois"

    # L'emprise au sol doit être celle demandée, à un pas près, et le modèle ne
    # doit pas dépasser la hauteur du mât.
    assert abs((haut[0] - bas[0]) - 2 * args.demi_x) < args.pas, "largeur est-ouest"
    assert abs((haut[2] - bas[2]) - 2 * args.demi_z) < args.pas, "largeur nord-sud"
    assert 0.0 < haut[1] - bas[1] < 300.0, "dénivelé du modèle hors de tout bon sens"

    # Le compte de triangles doit être celui que la boîte impose, à la
    # géométrie du mât près : deux par maille, plus les facettes de mur qui ne
    # partagent pas leurs sommets. Un écart signale un maillage abîmé, pas un
    # coût excessif.
    mailles = round(2 * args.demi_x / args.pas) * round(2 * args.demi_z / args.pas)
    assert 2 * mailles <= triangles <= 2 * mailles + 5000, (
        f"{triangles} triangles pour {mailles} mailles")
    # Le coût, lui, se juge au regard de ce qui a été mesuré : 166 412 triangles
    # coûtent 0,19 ms par image, soit 1,1 % du budget à 60 fps.
    if triangles > 400000:
        print(f"[contrôle] {triangles} triangles, soit environ "
              f"{0.19 * triangles / 166412:.2f} ms par image : "
              f"envisager --pas 1.5, qui divise par deux")

    # L'altitude déclarée dans la carte est celle du POINT LE PLUS BAS du
    # modèle : elle change dès que le maillage change, et un écart silencieux
    # enfoncerait ou ferait flotter tout l'observatoire.
    declaration = Path(config.TERRAIN_ROOT) / args.carte / "monuments.txt"
    if declaration.exists():
        for ligne in declaration.read_text(encoding="utf-8").splitlines():
            champs = ligne.split()
            if len(champs) > 7 and champs[7].endswith(f"{args.nom}.glb"):
                ecart = abs(float(champs[2]) - bas[1])
                assert ecart < 0.05, (f"{declaration} annonce {champs[2]} m, "
                                      f"le modèle commence à {bas[1]:.1f} m")
                print(f"[contrôle] {declaration.name} : altitude conforme "
                      f"({champs[2]} m)")
                break

    print(f"[contrôle] {chemin_glb.name} : {triangles} triangles, {sommets} sommets, "
          f"emprise {haut[0] - bas[0]:.0f} x {haut[2] - bas[2]:.0f} m, "
          f"dénivelé {haut[1] - bas[1]:.0f} m")
    print(f"[contrôle] texture {chemin_image.name} : "
          f"{Image.open(chemin_image).size[0]}x{Image.open(chemin_image).size[1]}")
    print("[contrôle] tout est conforme")
