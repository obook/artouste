#!/usr/bin/env python3
"""
convertir_3dexport.py
Convertit le modèle commercial 3dexport de l'Alouette II (Civil_FBX.FBX, hors
dépôt) en un .glb de cellule que le moteur charge à la place d'alouette.ac
(variable ARTOUSTE_MODELE). Seule la cellule est convertie : rotors, pilotes et
intérieur restent ceux du modèle FlightGear, posés par le moteur.

Le FBX est en centimètres, nez vers -Y, Z vers le haut. On le ramène en mètres
dans le repère du .ac (nez -X, Y haut, Z latéral), patins au sol et moyeu à
l'aplomb de MAIN_HUB (-2,142 ; 1,0 ; 0). Puis l'export glTF de Blender
tourne le tout dans sa convention (Y haut, nez +Z), que le moteur retourne
d'un quart de tour au dessin.

Les faces du matériau "glass" sont détachées dans un objet "verriere" : le
moteur rend translucides les noeuds dont le nom contient "verriere".
La texture du rotor (sa_313b_germany_c.jpg) manque de l'archive : on la
remplace par l'atlas civil, même dépliage. L'atlas intérieur agrandi en 4096
(interieur_4096.py), s'il est à côté du FBX, remplace celui d'origine.

Avec --poste, le poste du FBX est gardé (planche, consoles, commandes)
avec ses deux pilotes, en noeuds "equipage-droit" et "equipage-gauche", et la
tête du pilote à part ("tete-pilote"), et les commandes des deux sièges
(manches, collectif, palonniers) en noeuds "commande-*" que le moteur anime. Le moteur
ne pose alors plus les pièces FlightGear.

Usage :
    flatpak run org.blender.Blender -b --factory-startup \\
        -P tools/modele/convertir_3dexport.py -- Civil_FBX.FBX sortie.glb [--poste]

Auteur : O. Booklage
Date : novembre 2026
Licence : GPL v2
"""

import os
import sys

import bpy
import mathutils

ECHELLE = 100.0
# Moyeu du FBX en mètres, une fois mis à l'échelle (origine de Blurred_Main_Prop).
MOYEU_FBX = mathutils.Vector((0.04, -0.79, 2.55))
# Cible dans le repère du .ac : (latéral, longitudinal, haut) = (Z, X, Y).
# Latéral, X_ac, Y_ac. Le moyeu monte de 16 cm au-dessus du plan des pales du
# jeu (1,003) pour que les patins posent à -1,387 comme ceux du .ac ; le moyeu
# du jeu (mainrotor.ac) descend assez pour couvrir le raccord.
MOYEU_AC = mathutils.Vector((0.0, -2.142, 1.164))
CELLULE = ["fuse", "tail", "elevator", "gear_c", "gear_l", "gear_r",
           "blister1", "blister2"]
# Meubles du poste à retirer de "fuse" (le jeu pose les siens, animés) : un
# îlot du maillage dont la boîte tient dans l'une de ces boîtes, en mètres dans
# le repère du FBX (x latéral, y vers la queue, z haut), est supprimé. Les
# sièges et le plancher restent. Object001/002 ne sont jamais repris : c'est
# une tige à boîtier pendue au plafond, juste devant les yeux du pilote (deux
# copies superposées), absente de l'appareil réel.
POSTE = [
    ((-0.7, -3.5, 0.5), (0.7, -3.3, 0.7)),    # palonniers
    ((-0.5, -3.2, 0.5), (0.5, -3.0, 1.1)),    # manches cycliques et leurs embases
    ((0.1, -2.9, 0.5), (0.2, -2.3, 1.05)),    # levier de collectif
    ((-0.7, -3.4, 0.45), (0.7, -2.9, 1.5)),   # planche de bord et consoles
]
# Toujours retirés, absents de l'appareil réel et gênants pour la vue : un
# îlot dont la boîte tient dans l'une de ces boîtes et dont le point le plus
# bas est sous 'bas' est supprimé.
RETIRES = [
    # Barre en U au milieu de la cabine : descend du plafond derrière les
    # têtes, passe entre les pilotes à hauteur d'épaule, remonte vers la
    # verrière, avec le boîtier et la ferrure fixés dessus. 'bas' épargne les
    # petites pièces de l'arceau central prises dans la même boîte.
    (((0.0, -3.1, 1.35), (0.07, -2.35, 1.9)), 1.55),
    # Berceau devant le copilote : deux tubes sur ressorts portant un boîtier
    # à objectif. Le plancher de la boîte épargne les boutons du manche.
    (((0.0, -2.85, 1.235), (0.66, -2.66, 1.52)), 1.52),
]
# Compas magnétique (boîtier à fenêtre, repère 3 du manuel de vol), au-dessus
# du capot : il pend de sa jambe de force sous sa visière, mais tient dans la
# boîte de la barre en U ; on l'épargne.
COMPAS = ((0.0, -3.10, 1.43), (0.07, -3.0, 1.54))
# Repris avec --poste : les pilotes (un seul objet pour les deux).
PILOTES = "Pilots"
# Hauteur du cou du pilote, en mètres dans le repère recalé (patins à -1,387) :
# au-dessus, sa tête et son casque vont dans le noeud "tete-pilote", que le
# moteur ne dessine pas en vue cockpit (la caméra est dedans).
COU = 0.02
# Commandes conjuguées des deux sièges, détachées avec --poste pour que le
# moteur les anime : un îlot de "fuse" entièrement dans l'une des boîtes
# d'une commande (repère du FBX en mètres, x < 0 à droite) part dans un noeud
# à son nom. Les soufflets du manche et des pédales et le boîtier des
# collectifs restent soudés à la cellule.
COMMANDES = {
    # Tige en S, poignée, boutons et bague en haut du soufflet.
    "commande-manche": [((-0.42, -3.02, 0.585), (-0.28, -2.72, 1.25))],
    "commande-manche-copilote": [((0.33, -3.02, 0.585), (0.48, -2.72, 1.25))],
    # Levier bas à gauche de chaque siège : tige et poignée en T, sans le
    # boîtier du plancher où il s'articule.
    "commande-collectif": [((-0.045, -2.62, 0.57), (0.01, -2.44, 0.73))],
    "commande-collectif-copilote": [((0.60, -2.69, 0.57), (0.66, -2.50, 0.70))],
    # Étriers des pédales.
    "commande-palonnier-droit": [((-0.52, -3.30, 0.64), (-0.36, -3.22, 0.73))],
    "commande-palonnier-gauche": [((-0.32, -3.30, 0.64), (-0.16, -3.22, 0.73))],
    "commande-palonnier-droit-copilote": [((0.22, -3.30, 0.64), (0.39, -3.22, 0.73))],
    "commande-palonnier-gauche-copilote": [((0.42, -3.30, 0.64), (0.59, -3.22, 0.73))],
}
# Le levier de collectif du modèle est trop court : sa poignée arrive à
# 18 cm du plancher, celle de l'appareil réel au genou du pilote (photo
# d'une Alouette II de l'armée de Terre). On allonge sa tige de tant de
# mètres, dans son axe : la poignée garde sa forme et monte vers 40 cm.
ALLONGE_COLLECTIF = 0.30
# Membres du pilote, détachés pour que le moteur pose ses mains sur les
# commandes et ses pieds sur les pédales. Le mannequin est fait de petits
# blocs : chaque îlot va à la partie dont un os passe le plus près de son
# centre, à moins de son rayon. Os (articulations) dans le repère recalé
# (x latéral, y vers la queue, z haut), mesurés sur le .glb ; mêmes valeurs
# dans src/render/CommandesPoste.hpp.
EPAULE_G, COUDE_G, DOIGTS_G = (-0.19, -3.64, -0.04), (-0.15, -3.80, -0.29), (-0.17, -4.20, -0.32)
EPAULE_D, COUDE_D, DOIGTS_D = (-0.63, -3.60, -0.06), (-0.65, -3.75, -0.36), (-0.63, -4.15, -0.33)
HANCHE_G, GENOU_G = (-0.25, -3.76, -0.52), (-0.25, -4.12, -0.47)
CHEVILLE_G, ORTEILS_G = (-0.21, -4.43, -0.72), (-0.20, -4.62, -0.68)
HANCHE_D, GENOU_D = (-0.52, -3.76, -0.52), (-0.53, -4.12, -0.47)
CHEVILLE_D, ORTEILS_D = (-0.56, -4.42, -0.71), (-0.57, -4.58, -0.65)
MEMBRES_PILOTE = {
    "bras-g": ([(EPAULE_G, COUDE_G)], 0.065),
    "avantbras-g": ([(COUDE_G, DOIGTS_G)], 0.065),
    "bras-d": ([(EPAULE_D, COUDE_D)], 0.065),
    "avantbras-d": ([(COUDE_D, DOIGTS_D)], 0.065),
    "cuisse-g": ([(HANCHE_G, GENOU_G)], 0.09),
    "jambe-g": ([(GENOU_G, CHEVILLE_G), (CHEVILLE_G, ORTEILS_G)], 0.09),
    "cuisse-d": ([(HANCHE_D, GENOU_D)], 0.09),
    "jambe-d": ([(GENOU_D, CHEVILLE_D), (CHEVILLE_D, ORTEILS_D)], 0.09),
}


def couper(obj, nom, prendre):
    """Déplace dans un nouvel objet 'nom' les faces de 'obj' qui vérifient
    'prendre'."""
    import bmesh
    copie = obj.copy()
    copie.data = obj.data.copy()
    copie.name = nom
    bpy.context.scene.collection.objects.link(copie)
    for o, garder in ((copie, True), (obj, False)):
        bm = bmesh.new()
        bm.from_mesh(o.data)
        jeter = [f for f in bm.faces if prendre(f) != garder]
        bmesh.ops.delete(bm, geom=jeter, context="FACES")
        bm.to_mesh(o.data)
        bm.free()


def distance_os(p, a, b):
    """Distance du point p au segment [a, b]."""
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / ab.dot(ab)))
    return (p - (a + ab * t)).length


def partie_du_membre(c):
    """Partie du pilote où va un îlot de centre c ("bras-g", "jambe-d"...),
    ou None s'il reste au corps."""
    V = mathutils.Vector
    meilleure, distance = None, None
    for partie, (os_, rayon) in MEMBRES_PILOTE.items():
        d = min(distance_os(c, V(a), V(b)) for a, b in os_)
        if d < rayon and (distance is None or d < distance):
            meilleure, distance = partie, d
    return meilleure


def decouper_membres(pilote):
    """Sort du pilote ses bras, avant-bras, cuisses et jambes, chacun dans un
    noeud equipage-droit-<partie>."""
    import bmesh
    for partie in MEMBRES_PILOTE:
        # Îlots relus à chaque partie : couper renumérote les faces.
        bm = bmesh.new()
        bm.from_mesh(pilote.data)
        indices = set()
        for ilot, mn, mx in ilots(bm, echelle=1.0):
            centre = sum((v.co for v in ilot), mathutils.Vector()) / len(ilot)
            if partie_du_membre(centre) == partie:
                indices.update(f.index for v in ilot for f in v.link_faces)
        bm.free()
        couper(pilote, "equipage-droit-" + partie, lambda f, indices=indices: f.index in indices)
        print("convertir_3dexport : equipage-droit-%s, %d faces" % (partie, len(indices)))


def scinder_equipage(pilotes):
    """Coupe l'objet des pilotes en un noeud par siège, et la tête du pilote
    à part. Dans le repère du FBX, x < 0 est le côté droit, celui du pilote."""
    couper(pilotes, "equipage-gauche", lambda f: f.calc_center_median().x >= 0.0)
    couper(pilotes, "tete-pilote", lambda f: f.calc_center_median().z > COU)
    pilotes.name = "equipage-droit"
    decouper_membres(pilotes)


def ilots(bm, echelle=ECHELLE):
    """Îlots de sommets reliés de 'bm', chacun avec sa boîte (min, max) en
    mètres. Avant le recalage, les sommets sont cuits en repère monde à
    l'échelle 0,01 (matrix_world n'est pas encore rafraîchie et garderait
    celle du parent) ; après, ils sont en mètres : echelle=1."""
    bm.verts.ensure_lookup_table()
    vus = set()
    for v0 in bm.verts:
        if v0.index in vus:
            continue
        ilot = [v0]
        vus.add(v0.index)
        pile = [v0]
        while pile:
            v = pile.pop()
            for e in v.link_edges:
                w = e.other_vert(v)
                if w.index not in vus:
                    vus.add(w.index)
                    ilot.append(w)
                    pile.append(w)
        pts = [v.co * echelle for v in ilot]
        mn = [min(p[i] for p in pts) for i in range(3)]
        mx = [max(p[i] for p in pts) for i in range(3)]
        yield ilot, mn, mx


def dans_boite(mn, mx, boite):
    """Vrai si la boîte (mn, mx) d'un îlot tient dans 'boite'."""
    return all(boite[0][i] <= mn[i] and mx[i] <= boite[1][i] for i in range(3))


def detacher_commandes(fuse):
    """Sort de 'fuse' les îlots de chaque commande de COMMANDES, dans un
    objet à son nom. Renvoie ces objets."""
    import bmesh
    objets = []
    for nom, boites in COMMANDES.items():
        # Îlots relus à chaque commande : couper renumérote les faces.
        bm = bmesh.new()
        bm.from_mesh(fuse.data)
        indices = {f.index for ilot, mn, mx in ilots(bm)
                   if any(dans_boite(mn, mx, b) for b in boites)
                   for v in ilot for f in v.link_faces}
        bm.free()
        couper(fuse, nom, lambda f, indices=indices: f.index in indices)
        objets.append(bpy.data.objects[nom])
        print("convertir_3dexport : %s, %d faces" % (nom, len(indices)))
    return objets


def allonger_levier(obj, allonge):
    """Allonge de 'allonge' mètres la tige du levier 'obj' (l'îlot le plus
    long), dans son axe, depuis son bout bas qui reste en place ; les autres
    îlots (la poignée) glissent d'autant dans le même axe."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    morceaux = list(ilots(bm))
    tige = max(morceaux,
               key=lambda m: (mathutils.Vector(m[2]) - mathutils.Vector(m[1])).length)[0]
    bas = min(tige, key=lambda v: v.co.z).co.copy()
    haut = max(tige, key=lambda v: v.co.z).co.copy()
    axe = (haut - bas).normalized()
    longueur = (haut - bas).length
    allonge = allonge / ECHELLE  # sommets encore à l'échelle 0,01
    dans_tige = {v.index for v in tige}
    for v in bm.verts:
        t = (v.co - bas).dot(axe) / longueur if v.index in dans_tige else 1.0
        v.co += axe * allonge * max(0.0, min(1.0, t))
    bm.to_mesh(obj.data)
    bm.free()


def retirer_poste(fuse, avec_poste):
    """Supprime de 'fuse' les îlots de RETIRES et, sans 'avec_poste', ceux
    qui tiennent dans une boîte de POSTE."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(fuse.data)
    a_supprimer = []
    for ilot, mn, mx in ilots(bm):
        def dedans(boite):
            return dans_boite(mn, mx, boite)
        if dedans(COMPAS):
            continue
        if any(dedans(b) and mn[2] < bas for b, bas in RETIRES) or \
                (not avec_poste and any(dedans(b) for b in POSTE)):
            a_supprimer.extend(ilot)
    faces = {f for v in a_supprimer for f in v.link_faces}
    # Console centrale : son boîtier est soudé au plancher, l'îlot déborde de
    # toute boîte ; on coupe ses faces une à une dans la zone.
    for f in bm.faces if not avec_poste else []:
        c = f.calc_center_median() * ECHELLE
        if abs(c.x) < 0.35 and -3.45 <= c.y <= -2.95 and 0.45 <= c.z <= 1.5:
            faces.add(f)
    bmesh.ops.delete(bm, geom=list(faces), context="FACES")
    bm.to_mesh(fuse.data)
    bm.free()
    print("convertir_3dexport : %d faces retirées du poste" % len(faces))


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    entree, sortie = args[:2]
    avec_poste = "--poste" in args[2:]
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    bpy.ops.import_scene.fbx(filepath=entree)
    for im in bpy.data.images:
        if "germany" in im.name:
            im.filepath = os.path.join(os.path.dirname(entree), "sa_313b.jpg")
            im.reload()
        # Atlas intérieur agrandi par interieur_4096.py, s'il est à côté du FBX.
        net = os.path.join(os.path.dirname(entree), "sa_313b_interior_c_4096.jpg")
        if "interior" in im.name and os.path.exists(net):
            im.filepath = net
            im.reload()
            print("convertir_3dexport : atlas intérieur", net)

    # Ne garder que la cellule, en coordonnées monde (l'empty parent porte
    # l'échelle 0,01 et un quart de tour).
    noms = CELLULE + ([PILOTES] if avec_poste else [])
    garder = [o for o in bpy.data.objects if o.name in noms]
    for o in garder:
        o.data.transform(o.matrix_world)
        o.parent = None
        # Pas matrix_world = identité : l'export relirait position et rotation
        # locales encore en place et les écrirait dans le noeud glTF.
        o.location = (0.0, 0.0, 0.0)
        o.rotation_mode = "QUATERNION"
        o.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        o.rotation_mode = "XYZ"
        o.rotation_euler = (0.0, 0.0, 0.0)
        o.scale = (1.0, 1.0, 1.0)
        o.delta_location = (0.0, 0.0, 0.0)
        o.delta_rotation_euler = (0.0, 0.0, 0.0)
        o.delta_rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        o.delta_scale = (1.0, 1.0, 1.0)
        o.matrix_parent_inverse = mathutils.Matrix.Identity(4)
        # Le FBX porte une animation (pose à la première image) que l'export
        # glTF écrirait dans le noeud, par-dessus la matrice identité.
        o.animation_data_clear()
    for o in list(bpy.data.objects):
        if o not in garder:
            bpy.data.objects.remove(o)
    pilotes = bpy.data.objects.get(PILOTES) if avec_poste else None
    retirer_poste(bpy.data.objects["fuse"], avec_poste)
    commandes = detacher_commandes(bpy.data.objects["fuse"]) if avec_poste else []
    for o in commandes:
        if o.name.startswith("commande-collectif"):
            allonger_levier(o, ALLONGE_COLLECTIF)
    bpy.ops.object.select_all(action="DESELECT")
    for o in garder:
        o.select_set(o is not pilotes)
    bpy.context.view_layer.objects.active = bpy.data.objects["fuse"]
    bpy.ops.object.join()
    cellule = bpy.context.view_layer.objects.active
    cellule.name = "fuselage"
    # Mise à l'échelle et moyeu sur MAIN_HUB, dans le repère du FBX (x latéral,
    # y vers la queue, z haut). L'export glTF passe ensuite en Y haut, nez +Z :
    # (x, y, z) -> (x, z, -y), et le moteur fait le quart de tour restant.
    decalage = MOYEU_AC - MOYEU_FBX
    recalage = mathutils.Matrix.Translation(decalage) @ mathutils.Matrix.Scale(ECHELLE, 4)
    cellule.data.transform(recalage)
    for o in commandes:
        o.data.transform(recalage)
    if pilotes:
        pilotes.data.transform(recalage)
        scinder_equipage(pilotes)

    # Vitrages à part.
    idx = [i for i, m in enumerate(cellule.data.materials) if m and m.name == "glass"]
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    for p in cellule.data.polygons:
        p.select = p.material_index in idx
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.separate(type="SELECTED")
    bpy.ops.object.mode_set(mode="OBJECT")
    for o in bpy.data.objects:
        if o != cellule and not o.name.startswith(("equipage", "tete-pilote", "commande")):
            o.name = "verriere"
    print("convertir_3dexport : cellule %d faces, verrière %d faces" %
          (len(cellule.data.polygons), len(bpy.data.objects["verriere"].data.polygons)))

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=sortie, export_format="GLB", use_selection=True,
                              export_apply=True, export_image_format="JPEG", export_yup=True,
                              export_animations=False)
    print("convertir_3dexport : écrit", sortie)


if __name__ == "__main__":
    main()
