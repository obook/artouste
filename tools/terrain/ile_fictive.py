"""Île volcanique fictive : relief, orthophoto et calage.

Carte d'essai du chemin heightmap.bin. Rien d'IGN ici, tout est inventé : le
relief vient d'un cône déformé par du bruit à crêtes, puis érodé. Sert à
vérifier que le moteur lit bien un heightmap.bin qu'aucun script IGN n'a
produit.

Usage : python3 tools/terrain/ile_fictive.py [dossier de sortie]
"""

import sys
import numpy as np
from PIL import Image
from pathlib import Path

CIBLE = Path(sys.argv[1] if len(sys.argv) > 1 else "assets/terrain/ile-volcan")
N = 1024
COTE_M = 12000.0
SOMMET = 1480.0

rng = np.random.default_rng(20260912)


def valeur(cote):
    """Une nappe de bruit lisse, interpolée depuis une grille grossière."""
    grille = rng.random((cote, cote)).astype(np.float32)
    return np.array(Image.fromarray(grille).resize((N, N), Image.BICUBIC))


def fbm(octaves=7, cote0=3):
    """Bruit fractal classique : doux de loin, détaillé de près."""
    total, amplitude, poids = 0.0, 1.0, 0.0
    for k in range(octaves):
        total += amplitude * valeur(cote0 * 2 ** k)
        poids += amplitude
        amplitude *= 0.5
    return total / poids


def ridged(octaves=7, cote0=3):
    """Bruit à crêtes : c'est lui qui creuse les ravines d'un flanc de volcan."""
    total, amplitude, poids = 0.0, 1.0, 0.0
    for k in range(octaves):
        v = valeur(cote0 * 2 ** k)
        total += amplitude * (1.0 - np.abs(2.0 * v - 1.0)) ** 2
        poids += amplitude
        amplitude *= 0.5
    return total / poids


def erosion_thermique(h, passes=40, talus=0.55, part=0.22):
    """Les pentes trop raides s'éboulent : le tas glisse vers le voisin bas."""
    pas = COTE_M / (N - 1)
    seuil = talus * pas
    for _ in range(passes):
        depot = np.zeros_like(h)
        for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            voisin = np.roll(np.roll(h, dj, axis=0), di, axis=1)
            ecart = h - voisin
            bouge = np.where(ecart > seuil, (ecart - seuil) * part * 0.25, 0.0)
            depot -= bouge
            depot += np.roll(np.roll(bouge, -dj, axis=0), -di, axis=1)
        h = h + depot
    return h


axe = np.linspace(-COTE_M / 2, COTE_M / 2, N)
x, z = np.meshgrid(axe, axe)

# Le cône, décalé du centre. Le rayon est déformé par du bruit : sans cela
# tout resterait un disque parfait, et ça se voit tout de suite.
cx, cz = 600.0, -300.0
deformation = (fbm(5, 2) - 0.5) * 2.0
r = np.hypot(x - cx, z - cz) * (1.0 + 0.22 * deformation)

RAYON = 5000.0
flanc = np.clip(1.0 - r / RAYON, 0.0, 1.0)
h = SOMMET * flanc ** 1.55

# Ravines : les crêtes du bruit, marquées sur le flanc et effacées au
# sommet comme au bord de l'eau.
creux = (1.0 - ridged(7, 4)) * np.clip(r / 900.0, 0.0, 1.0) * flanc ** 0.5
h -= 300.0 * creux

# Cratère : une lèvre nette, un plancher plat.
RC = 560.0
h -= 430.0 * np.clip(1.0 - r / RC, 0.0, 1.0) ** 0.55
h += 55.0 * np.clip(1.0 - np.abs(r - RC) / 190.0, 0.0, 1.0)

# Cône parasite sur le flanc sud-ouest, avec son propre petit cratère.
r2 = np.hypot(x + 2500.0, z - 2200.0) * (1.0 + 0.25 * (fbm(4, 3) - 0.5))
h += 330.0 * np.clip(1.0 - r2 / 1400.0, 0.0, 1.0) ** 1.4
h -= 150.0 * np.clip(1.0 - r2 / 300.0, 0.0, 1.0) ** 0.6

# Détail général.
h += (fbm(8, 4) - 0.5) * 130.0 * np.clip(h / SOMMET, 0.08, 1.0) ** 0.4

# La côte, elle aussi déformée : des caps et des baies, pas un cercle.
cote = 4500.0 + 1400.0 * (fbm(4, 2) - 0.5) * 2.0
h -= 420.0 * np.clip((r - cote) / 700.0, 0.0, 4.0)

h = erosion_thermique(h.astype(np.float32))

# Lagon au sud-est, fermé par un cordon de sable.
rl = np.hypot(x - 2600.0, z - 2900.0)
h -= 70.0 * np.clip(1.0 - rl / 850.0, 0.0, 1.0) ** 0.7
h += 30.0 * np.clip(1.0 - np.abs(rl - 950.0) / 140.0, 0.0, 1.0)

# La mer est le zéro : le moteur dessine l'eau en dessous.
h = np.maximum(h, 0.0).astype(np.float32)

CIBLE.mkdir(parents=True, exist_ok=True)
h.tofile(CIBLE / "heightmap.bin")

# ---------------------------------------------------------------- orthophoto
gz, gx = np.gradient(h, COTE_M / (N - 1))
pente = np.hypot(gx, gz)
ombre = np.clip(0.58 + 0.62 * ((-gx + gz) / np.maximum(pente, 1e-4) * 0.5 + 0.5), 0.4, 1.3)
ombre = np.where(pente < 1e-4, 1.0, ombre)

paliers = [
    (0.0,    (24, 64, 100)),
    (1.5,    (216, 200, 162)),
    (18.0,   (120, 140, 78)),
    (240.0,  (64, 98, 54)),
    (560.0,  (86, 84, 64)),
    (900.0,  (74, 68, 62)),
    (1150.0, (54, 48, 46)),
    (1450.0, (96, 58, 48)),
]
niveaux = np.array([p[0] for p in paliers])
couleurs = np.array([p[1] for p in paliers], dtype=float)
rvb = np.stack([np.interp(h, niveaux, couleurs[:, c]) for c in range(3)], axis=-1)

nu = np.clip((pente - 0.5) / 0.55, 0.0, 1.0)[..., None]
rvb = rvb * (1.0 - nu) + np.array([100.0, 92.0, 84.0]) * nu

# Coulées de lave refroidies : des traînées sombres depuis le cratère.
trainee = np.clip((ridged(6, 5) - 0.62) / 0.2, 0.0, 1.0) * np.clip(1.0 - r / 4200.0, 0.0, 1.0)
rvb = rvb * (1.0 - 0.55 * trainee[..., None]) + np.array([44.0, 38.0, 36.0]) * 0.55 * trainee[..., None]

grain = (fbm(6, 8)[..., None] - 0.5) * 24.0
rvb = np.clip((rvb + grain) * ombre[..., None], 0, 255)
rvb[h <= 0.0] = np.array([24, 64, 100])

Image.fromarray(rvb.astype(np.uint8)).resize((2048, 2048), Image.LANCZOS).save(
    CIBLE / "ortho.jpg", quality=92)

# ------------------------------------------------------------------- calage
LON0, LAT0 = -25.42, 37.75
dlon = COTE_M / (111320.0 * np.cos(np.radians(LAT0))) / 2.0
dlat = COTE_M / 110540.0 / 2.0


def altitude(mx, mz):
    ix = int(round((mx + COTE_M / 2) / COTE_M * (N - 1)))
    iz = int(round((mz + COTE_M / 2) / COTE_M * (N - 1)))
    return float(h[iz, ix])


def plus_plat(masque):
    """Le point le plus plat d'une zone : un pad ne se pose pas en devers."""
    idx = np.argwhere(masque)
    if len(idx) == 0:
        return None
    jz, jx = idx[np.argmin(pente[masque])]
    return float(x[jz, jx]), float(z[jz, jx])


# Trois pads : le départ sur la plage, une épaule à mi-flanc, le lagon.
depart = plus_plat((h > 4.0) & (h < 30.0) & (x < -2000.0) & (np.abs(z) < 3000.0))
epaule = plus_plat((h > 620.0) & (h < 780.0) & (pente < 0.25) & (z < 0))
lagon = plus_plat((h > 3.0) & (h < 25.0) & (x > 1500.0) & (z > 1800.0))
sx, sz = depart
print(f"départ ({sx:.0f}, {sz:.0f}) à {altitude(sx, sz):.1f} m")
print(f"sommet {h.max():.0f} m, terres émergées {100.0 * (h > 0).mean():.0f} %")

(CIBLE / "terrain.txt").write_text(
    "# Terrain Artouste - île volcanique FICTIVE (carte d'essai, aucune donnée réelle)\n"
    "# Relief et orthophoto engendrés par tools/terrain/ile_fictive.py ; l'emprise\n"
    "# est posée au large des Açores pour donner une échelle, rien de plus.\n"
    f"cols {N}\nrows {N}\n"
    f"width_m {COTE_M:.1f}\nheight_m {COTE_M:.1f}\n"
    f"elev_min 0.00\nelev_max {h.max():.2f}\n"
    f"lon_min {LON0 - dlon:.6f}\nlon_max {LON0 + dlon:.6f}\n"
    f"lat_min {LAT0 - dlat:.6f}\nlat_max {LAT0 + dlat:.6f}\n"
    "ortho_width 2048\northo_height 2048\n"
    "sea 1\n"
    f"start_x {sx:.1f}\nstart_z {sz:.1f}\nstart_heading 75\n"
    "origin_x 0.00\norigin_z 0.00\n")


def vers_geo(mx, mz):
    return (LON0 + mx / (COTE_M / 2) * dlon, LAT0 - mz / (COTE_M / 2) * dlat)


reperes = [(cx, cz, "Cratère du Piton"), (-2500.0, 2200.0, "Cône parasite"),
           (2600.0, 2900.0, "Lagon sud-est"), (sx, sz, "Plage ouest"),
           (0.0, -4600.0, "Pointe nord")]
lignes = ["# Lieux remarquables - île volcanique fictive (un par ligne : lon lat nom)\n"]
for mx, mz, nom in reperes:
    lon, lat = vers_geo(mx, mz)
    lignes.append(f"{lon:.6f} {lat:.6f} {nom}\n")
(CIBLE / "landmarks.txt").write_text("".join(lignes))

# Hélipads : le départ s'y cale tout seul, le moteur aplanit la maille dessous.
pads = [(depart, "Plage ouest (départ)"), (epaule, "Épaule du Piton"),
        (lagon, "Lagon sud-est")]
lignes = ["# Hélipads - île volcanique fictive (un par ligne : lon lat nom)\n"]
for pos, nom in pads:
    if pos is None:
        continue
    lon, lat = vers_geo(*pos)
    lignes.append(f"{lon:.6f} {lat:.6f} {nom}\n")
(CIBLE / "helipads.txt").write_text("".join(lignes))

# Balise HAPI sur le pad de départ : on arrive du large, cap à l'est.
lon, lat = vers_geo(*depart)
(CIBLE / "hapi.txt").write_text(
    "# Balises HAPI - île volcanique fictive "
    "(un par ligne : lon lat azimut_deg pente_pct nom)\n"
    f"{lon:.6f} {lat:.6f} 75 6 Plage ouest (départ)\n")

print("écrit dans", CIBLE)
