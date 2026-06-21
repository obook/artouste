# Terrain : choix actuel et autres pistes

Étude comparative des approches possibles pour le terrain d'Artouste, par rapport à
la solution en place. But : éclairer les évolutions futures sans casser ce qui
marche.

## Le choix actuel

Données : IGN Géoplateforme, gratuit (Licence Ouverte Etalab 2.0), France uniquement.

- Relief : RGE ALTI, via l'API altimétrie REST (`elevation.json`), échantillonné
  point par point sur une grille 512x512 (un appel par rangée, voir
  `tools/fetch_terrain.py`).
- Texture : BD ORTHO, via le service WMS (`ORTHOIMAGERY.ORTHOPHOTOS`), une seule
  image `ortho.jpg` (2064x2048) drapée sur le relief.

Pipeline : script Python hors-ligne -> `assets/terrain/<zone>/{heightmap.png
(16 bits), ortho.jpg, terrain.txt, landmarks.txt, helipads.txt (facultatif)}`.
Chaque terrain a son propre
sous-dossier (par exemple `ossau/` ou `cote-landes/`) ; celui chargé au lancement
est choisi par la clé `terrain` de `assets/config.txt` (ou la variable
d'environnement `ARTOUSTE_TERRAIN`). Au runtime (`src/render/Terrain.cpp`) :
`stb_image` charge tout, un seul maillage 512x512 (~520k triangles), une seule
texture drapée, les altitudes restent en RAM pour le contact sol (`heightAt`),
repli sur un damier plat si les données manquent.

Le script `tools/fetch_terrain.py` prend le nom de la zone en argument
(`python3 tools/fetch_terrain.py cote-landes`) ; les zones sont décrites dans le
dictionnaire `ZONES` en tête du script (bornes, mer ou montagne, point de départ,
lieux remarquables). Ajouter une zone = copier une entrée et changer les bornes.

Emprise : ~17,9 x 17,8 km (vallée d'Ossau : lac d'Artouste, pic du Midi d'Ossau,
0 à 2937 m). Terrain de montagne, sans mer.

Conséquences chiffrées :

- Maille ~35 m (17900 / 511). Le relief reste lissé par rapport à la source native
  (RGE ALTI à 1-5 m), mais le pic du Midi d'Ossau et la vallée sont bien marqués.
  La limite vient de la grille, pas de la source.
- Texture ~9 m/pixel (correcte vue d'hélico).
- Aucune dépendance lourde au runtime (seulement `stb_image`). Code court, lisible.
- Zone figée et petite : au-delà de l'emprise, le sol est plat au niveau 0.

## Les autres pistes, comparées

### Axe 1 - Source du relief (DEM)

| Piste | Résolution | Couverture | Licence | vs nous |
|-------|-----------|-----------|---------|---------|
| RGE ALTI (actuel) | 1 m / 5 m natif | France | gratuit Etalab | déjà excellent, on le sous-échantillonne à 256 |
| Copernicus DEM GLO-30 | 30 m | Monde | gratuit | mondial, mais 3x plus grossier que RGE ALTI en France |
| SRTM | 30 m (~90 m hors USA) | quasi-monde | gratuit | vieux, trous en montagne |
| Copernicus GLO-90 / ASTER | 90 m | Monde | gratuit | trop grossier pour la montagne |

RGE ALTI est la meilleure source pour la France (donc Pyrénées, dont la vallée
d'Ossau, et Alpes). Copernicus n'a d'intérêt que pour sortir de France.

### Axe 2 - Source de la texture

| Piste | Résolution | Couverture | Licence | vs nous |
|-------|-----------|-----------|---------|---------|
| BD ORTHO (actuel) | ~20 cm natif | France | gratuit Etalab | déjà très bon |
| IGN WMTS | idem BD ORTHO | France | gratuit (clé) | même donnée, en tuiles pré-découpées (utile pour le LOD) |
| Sentinel-2 (ESA) | 10 m | Monde | gratuit | mondial mais flou de près |
| Mapbox / Bing satellite | ~0,5-1 m | Monde | payant / restrictif | mondial mais licence à surveiller |

BD ORTHO reste le meilleur choix en France. WMTS n'apporte que si on passe en
tuiles/LOD.

### Axe 3 - Pipeline (préparation des données)

| Piste | Dépendances | vs nous |
|-------|-------------|---------|
| API REST + WMS + stb_image (actuel) | urllib, Pillow (hors-ligne) ; stb_image (runtime) | léger, lisible par un élève |
| GDAL | GDAL (grosse lib C++ + données) | lecture GeoTIFF, reprojection, découpe ; lourd à installer/compiler |

GDAL ne se justifie que si on passe au DEM mondial GeoTIFF (Copernicus) ou aux gros
jeux multi-tuiles. Pour rester sur l'IGN, le pipeline actuel suffit et respecte la
contrainte "code simple".

### Axe 4 - Rendu (le vrai levier d'amélioration)

| Piste | Idée | Gain | Coût |
|-------|------|------|------|
| Tuile unique fixe (actuel) | 1 maillage, 1 texture | très simple | zone petite, maille grossière |
| Maille plus fine | passer 256 -> 512 / 1024 | relief net | + RAM/triangles, mais gérable |
| Tuiles + LOD (quadtree / geo-clipmaps) | charger autour de l'appareil, simplifier au loin | grande zone + relief fin | nettement plus de code |
| Tessellation GPU | subdiviser la grille dans le shader | détail sans grosse RAM | shaders plus avancés |

## Recommandation

Le choix actuel (IGN RGE ALTI + BD ORTHO, pré-cuit, mono-tuile) est le bon pour le
périmètre France et l'esprit "simple et lisible". Les sources sont déjà les
meilleures disponibles gratuitement ; la limite n'est ni le DEM ni l'ortho, c'est
le rendu mono-tuile en 256x256.

Par ordre d'effort croissant :

1. Quick win : monter la grille (512 ou 1024) et au besoin agrandir l'emprise dans
   `fetch_terrain.py`. Relief bien plus net, zéro nouvelle dépendance, code
   inchangé. Il suffit de changer `COLS` et `ROWS` (et éventuellement les bornes
   `LON/LAT` pour viser les Pyrénées).
2. Moyen terme : passer en tuiles + LOD (geo-clipmaps), en gardant l'IGN comme
   source. Grande zone survolable. Plus de code, mais pas de GDAL.
3. Seulement pour quitter la France : Copernicus DEM + GDAL + texture mondiale.
   C'est la seule piste qui justifie GDAL.

Tant qu'on reste sur les terrains où volait l'Alouette gendarmerie (Pyrénées,
Alpes, côtes françaises), rester sur l'IGN et investir dans le rendu (grille fine
puis LOD) donne un meilleur résultat pour moins de complexité.
