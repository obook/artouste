# ROADMAP

## Super HUD

- [x] Superposer à l'image des instruments à définir en vert transparent uniquement pour ne par géner la vue afin de permettre un pilotage réaliste, la liste des instruments est à rechercher dans docs/tableau-de-bord, l'implémentation se fait un par un, raccourci clavier/manette cyclique hud au 4 coins, hud superposé, pas de hud.

Liste des instruments par priorité : voir Priorité 1 du fichier PANEL.md

## Réalisme

- [ ] Le tableau de bord est celui qui est standard FlightGear, le changer avec celui sui est préparé dans assets/models/Alouette-II-panel

- [ ] Étudier la possibilité de recevoir des messages radio (pré-enregistrés, synthèse vocale)

- [ ] Ajouter les livées Armée de l'Air, Armée de Terre (ALAT), Marine nationale, Sécurité civile, Belgique (ALFT), Suisse : rotation par changement de livrée -> le rotor de queue  + anneau de sécurité  est le même pour tous.

- [ ] Vérifier la livrée du rotor principal, il semble qu'il n'y a aucune, donc faire en gris foncé

### Sons

- [ ] Retravailler les sons, en ajoutant ceux d'Émmanuel.

- [ ] Au débranchement qu'un casque USB, le son ne revient pas à la sortie principale

- [ ] Prévoir dans le fichier de configuration une URL pour un flux radio + commandes radio on/off et mixage Heli/Music (balance entre les deux)


### Mode demo

**Route de la démo** (parcours du pilote automatique ; corriger l'ordre et les altitudes ici, c'est la référence) :

1. Décollage du pad (aérodrome de La Teste).
2. Survol de la Dune du Pilat à 2000 m (passage haut, panorama).
3. Survol du cap Ferret par son nord en rase-mottes à 30 m.
4. Survol d'Arcachon à 1000 m.
5. Retour au pad et pose.

> Note : altitude = hauteur de vol (au-dessus du bassin et du littoral bas, proche de l'altitude mer). Arcachon vient des lieux remarquables du terrain (landmarks.txt) ; la Dune du Pilat (lat 44.5846722, lon -1.2075204) et le cap Ferret par son nord (lat 44.6634685, lon -1.2582492) sont des coordonnées explicites.

- [ ] Mode démo : à l'atterrissage on voit 2 helipads proches, un a son texte ou l'hélico n'attérit pas et l'autre où l'hélico attérit n'en a pas (pas toujours reproductible)

- [ ] Mode démo : décollage encore plus lent et doux, leger piqué avant pour avancer

- [ ] On ne sait pas quand et de quelle manière on est sorti du mode démo, trouver une solution

## Terrain

> Réalisé, mais par une autre voie que les pistes ci-dessous : terrain réel de la
> vallée d'Ossau (lac d'Artouste, pic du Midi d'Ossau) issu de l'IGN Géoplateforme
> (relief RGE ALTI, texture BD ORTHO), via le script hors-ligne
> `tools/fetch_terrain.py` qui produit `assets/terrain/{heightmap.png, ortho.jpg,
> terrain.txt}`. Au runtime, `stb_image` charge le tout (sans GDAL), un maillage
> unique est drapé de l'orthophoto, les altitudes servent au contact sol. Détails
> et comparaison des autres pistes dans `docs/TERRAIN.md`. Les pistes notées plus
> bas (Copernicus DEM, GDAL, LOD) restent des évolutions possibles.
>
> Plusieurs maps sont désormais livrées et choisies par la clé `terrain` de la
> configuration : `ossau` (montagne), `cote-landes` (côte basco-landaise),
> `arcachon` (bassin d'Arcachon, du Cap Ferret à Marcheprime, de Biscarrosse à Arès)
> et `cauterets` (Cauterets - Gavarnie : chemin des cascades, Pont d'Espagne, lac de
> Gaube sous le Vignemale, cirque de Gavarnie ; montagne, `height_min` à 0 pour
> garder cabanes et refuges).

Pour le terrain en C++/OpenGL, les pistes les plus directes :
- [x] Données d'élévation

Le Copernicus DEM (GLO-30, résolution 30 m) couvre l'Europe entière et est en accès libre. Pour une zone Pyrénées/Alpes où l'Alouette II opérait en sauvetage montagne, c'est la source la plus précise disponible gratuitement. Les fichiers sont au format GeoTIFF, lisibles en C++ avec la bibliothèque libgeotiff ou GDAL.

- [x] Pipeline terrain typique en C++/OpenGL
GeoTIFF (Copernicus DEM)
  -> GDAL (lecture, reprojection, extraction zone)
  -> Heightmap (tableau float 2D)
  -> Génération mesh OpenGL (grille de triangles)
  -> Texture satellite (Mapbox, WMTS IGN Géoportail)
  -> Rendu avec LOD (Level of Detail) selon distance caméra
  
- [x] Librairies C++ utiles
GDAL : lecture de tous les formats géographiques, incontournable.
GLM : mathématiques 3D pour OpenGL.
stb_image : chargement des textures.
Dear ImGui : interface HUD en overlay OpenGL, très utilisé dans les simulateurs.

IGN Géoportail
Pour les textures de sol sur la France, l'IGN propose des flux WMTS gratuits (orthophotos, cartes IGN) accessibles via une clé API gratuite. La couverture Pyrénées est excellente.

### Sources de terrain possibles

Liste des sources envisageables pour produire de nouveaux terrains (relief +
texture). L'actuelle est l'IGN ; les autres sont des pistes pour étendre la zone
ou sortir de France.

- IGN Géoplateforme (actuel) : RGE ALTI (relief 1-5 m) + BD ORTHO (orthophoto),
  France uniquement, gratuit (Licence Ouverte Etalab 2.0). Le script
  `tools/fetch_terrain.py` génère n'importe quelle emprise française en changeant
  les bornes lon/lat. C'est la meilleure qualité pour les Pyrénées et les Alpes.
- Scènes FlightGear (TerraSync) : décor mondial libre, déjà cohérent avec le projet
  (le modèle et les sons de l'Alouette viennent de FlightGear). Relief et sol sont
  stockés en tuiles BTG (géométrie TerraGear) avec des fichiers de placement STG,
  construits à partir de SRTM et d'une classification de sol (CORINE / OSM). Deux
  voies : extraire l'altitude des BTG pour alimenter notre heightmap, ou charger
  directement les maillages BTG. Couverture mondiale, mais relief plus grossier que
  l'IGN en France.
- Copernicus DEM GLO-30 : relief mondial à 30 m, gratuit, format GeoTIFF (lecture
  via GDAL ou libgeotiff). Utile pour une zone hors de France.
- SRTM (30 m, ~90 m hors USA) : relief quasi mondial, gratuit, mais ancien et
  troué en haute montagne.
- OpenStreetMap + SRTM (chaîne TerraGear / osm2city) : pour fabriquer soi-même des
  scènes façon FlightGear, avec bâtiments et réseau routier.
- Tuiles terrain-RGB et satellite (Mapbox, MapTiler) : relief encodé en PNG et
  imagerie mondiale, simples à draper, mais sous conditions d'utilisation et clé API.


### Bâtiments 3D

- [x] Bâtiments en volume sur les terrains de plaine (ville côtière). Emprises au
  sol de la BD TOPO de l'IGN (WFS), extrudées à leur hauteur réelle (murs + toit
  plat), murs clairs et toits tuile. Outil hors-ligne `tools/fetch_buildings.py`
  (filtre les constructions < 2 m) -> `assets/terrain/<zone>/buildings.bin` ;
  rendu par `render::Buildings` en un maillage statique unique, éclairé et noyé
  dans la même brume que le terrain. Côte basco-landaise : ~156 000 bâtiments ;
  bassin d'Arcachon : ~187 000 bâtiments.

    Pistes plus tard : variété des toits (plat/2 pentes selon la nature BD TOPO),
  niveaux de détail (LOD) pour alléger les grandes villes, bâtiments sur Ossau.
  
- [ ] Mettre aussi les petits bâtiments pour toutes les cartes avec des montagnes ou peu de villes

## Quelques observations à traiter

- [ ] FUEL_BURN_MAX_LPH = 194.0f : nos fiches indiquent 155 kg/h à puissance maxi. Avec kérosène à 0,8 kg/L, cela donne environ 194 L/h. La conversion est juste.

- [ ] MASS = 1100.0f : les fiches indiquent 895 kg à vide. 1 100 kg correspond à une masse en charge raisonnable, cohérent avec un pilote + carburant.

- [ ] LEVEL_GAIN = 6000.0f : c'est le rappel artificiel vers l'horizontale mentionné dans les constantes. Il est honnêtement documenté comme une aide non réaliste. À réduire progressivement quand le pilotage sera maîtrisé.

- [ ] HudMode::Overlay avec "instruments ronds verts superposés (Super HUD)" : ce mode n'est pas dans nos fiches. C'est une bonne idée, à documenter dans PANEL.md.

- [ ] Mettre un PDF du README dans les archives des binaires dans les releases

### Sons

- [ ] pendant le début de montée de la rurbine, j'entends les pales => mettre le son des pales lorsque elles ont à 70% de 360 tr/min

### Manuel

- [ ] Fournir un PDF propre et automatiquement à jour du readme dans les artéfacts Linux et Windows (.tar.gz et .zip) issu du README.md afin de guider l'utilisateur sur le fonctionnement.
