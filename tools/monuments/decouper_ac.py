#!/usr/bin/env python3
"""Découpe un modèle AC3D en deux selon une région rectangulaire du plan XZ.

Pourquoi cet outil. Les modèles de scène FlightGear ne placent pas toujours
correctement leurs éléments les uns par rapport aux autres. L'Hôtel des
Invalides en est l'exemple : aucune pose unique ne peut satisfaire à la fois le
corps du complexe et son église du Dôme, décalés d'une quinzaine de mètres l'un
par rapport à l'autre dans le fichier d'origine. On sépare donc la partie
fautive pour lui donner sa propre ligne dans monuments.txt.

Usage :
    decouper_ac.py source.ac reste.ac region.ac x0 x1 z0 z1

Les bornes sont en mètres, relatives au CENTRE de la boîte englobante du modèle,
qui est aussi le point sur lequel le moteur recentre (voir
ApplicationMonuments.cpp). Une surface part dans region.ac si son centre de
gravité tombe dans le rectangle, sinon dans reste.ac ; les sommets sont
renumérotés de part et d'autre.

La découpe des Invalides s'est faite ainsi :
    decouper_ac.py invalides-ba.ac invalides-corps-ba.ac \
                   invalides-eglise-ba.ac -115 0 -8 58

Auteur : O. Booklage
Date : juillet 2026
Licence : GPL v2
"""
import sys

def lire(chemin):
    lignes = open(chemin, errors='replace').read().split('\n')
    i = 0
    entete, sommets, surfaces = [], [], []
    while i < len(lignes):
        l = lignes[i]
        if l.startswith('numvert'):
            n = int(l.split()[1])
            for k in range(1, n + 1):
                sommets.append(tuple(map(float, lignes[i + k].split())))
            i += n + 1
            continue
        if l.startswith('numsurf'):
            n = int(l.split()[1])
            i += 1
            for _ in range(n):
                bloc = []
                while not lignes[i].strip().startswith('refs'):
                    bloc.append(lignes[i]); i += 1
                nref = int(lignes[i].split()[1]); i += 1
                refs = []
                for _ in range(nref):
                    p = lignes[i].split()
                    refs.append((int(p[0]), p[1], p[2])); i += 1
                surfaces.append((bloc, refs))
            continue
        entete.append(l); i += 1
    return entete, sommets, surfaces

def ecrire(chemin, entete, sommets, surfaces, garder):
    pris = [s for k, s in enumerate(surfaces) if garder[k]]
    idx = sorted({r[0] for _, refs in pris for r in refs})
    renum = {v: n for n, v in enumerate(idx)}
    out = []
    for l in entete:
        if l.startswith('kids 0'):
            break
        out.append(l)
    out.append('numvert %d' % len(idx))
    for v in idx:
        out.append('%g %g %g' % sommets[v])
    out.append('numsurf %d' % len(pris))
    for bloc, refs in pris:
        out += bloc
        out.append('refs %d' % len(refs))
        for v, u, w in refs:
            out.append('%d %s %s' % (renum[v], u, w))
    out.append('kids 0')
    open(chemin, 'w').write('\n'.join(out) + '\n')
    return len(idx), len(pris)

src, dstA, dstB = sys.argv[1], sys.argv[2], sys.argv[3]
x0, x1, z0, z1 = map(float, sys.argv[4:8])
entete, sommets, surfaces = lire(src)
xs = [s[0] for s in sommets]; zs = [s[2] for s in sommets]
cx = (min(xs) + max(xs)) / 2; cz = (min(zs) + max(zs)) / 2
print('%d sommets, %d surfaces ; centre de boite X%+.1f Z%+.1f' % (len(sommets), len(surfaces), cx, cz))

dansRegion = []
for bloc, refs in surfaces:
    mx = sum(sommets[r[0]][0] for r in refs) / len(refs) - cx
    mz = sum(sommets[r[0]][2] for r in refs) / len(refs) - cz
    dansRegion.append(x0 <= mx <= x1 and z0 <= mz <= z1)

nv, ns = ecrire(dstA, entete, sommets, surfaces, [not d for d in dansRegion])
print('  reste   : %4d sommets, %4d surfaces -> %s' % (nv, ns, dstA))
nv, ns = ecrire(dstB, entete, sommets, surfaces, dansRegion)
print('  region  : %4d sommets, %4d surfaces -> %s' % (nv, ns, dstB))
