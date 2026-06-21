# ROADMAP

## Plateforme

- [x] Portage Windows livré

    Code rendu portable (recherche du dossier du binaire via GetModuleFileNameW sous
    Windows), build CMake autonome (dépendances et runtime MSVC en statique, GLAD
    pré-généré dans `third_party/`). Validé : la version Windows compile et tourne,
    tests 100% fonctionnels (suite Catch2 verte sur Windows).

- [x] Release automatisée Linux et Windows livrée

    CI GitHub Actions (`.github/workflows/release.yml`) : au push d'un tag `v*`, elle
    compile Linux et Windows, teste, empaquette et publie une release GitHub avec les
    deux archives autonomes (binaire + ressources complètes). Première release :
    `v0.1.0`.

## Super HUD

- [x] Superposer à l'image des instruments à définir en vert transparent uniquement pour ne par géner la vue afin de permettre un pilotage réaliste, la liste des instruments est à rechercher dans docs/tableau-de-bord, l'implémentation se fait un par un, raccourci clavier/manette cyclique hud au 4 coins, hud superposé, pas de hud.

Liste des instruments par priorité : voir Priorité 1 du fichier PANEL.md

- [x] Mettre l'altitude dans le même style que la boussole, mais à gauche et verticalement

## Réalisme

- [x] Configuration

    Module de chargement de la configuration

    Fichier `assets/config.txt` au format "clé valeur" (parser maison, comme
    `terrain.txt`), chargé et appliqué au lancement, éditable à la main (voir
    `src/app/Config.cpp`). Première clé en place : `terrain` (choix de la map).
    La variable d'environnement `ARTOUSTE_TERRAIN` a la priorité. Reste à exposer
    en clés : la position de la caméra intérieure, et d'autres réglages au besoin.

- [x] balise de sécurité est un feu clignotant rouge situé sur le fuselage (strombo) : l'aspect est celui d'un cylindre avec le premier quart du haut clignotant en rouge vif clignotant (l'original est en rotation, voir 21-165-05.webp)

- [x] tuyère : Distorsion thermique visuelle localisée par l'air chaud, léger halo bleuté comme sur une gazinière à feu bas

- [x] Mode assisté : couche de confort (touche M / croix haut), voir src/physics/FlightAssist

- [x] L'ombre des pales est centrée sur le centre de gravité, il faut le centrer sur le rotor principal

- [ ] Il manque le skin du rotor arrière

- [x] Il manque une map pour se situer

- [ ] Étudier la possibilité de recevoir des messages radio (pré-enregistrés, synthèse vocale)

### Ombre hélicopter

- [ ] Elle est trop foncée/sombre

### Turbine

- [x] Il faut un décalage de quelques secondes entre la turbine max et le laché de frein qui permet la rotation des pales

### Sons

- [ ] Retravailler les sons, en ajoutant ceux d'Émmanuel.

- [ ] Au débranchement qu'un casque USB, le son ne revient pas à la sortie principale 

## Terrain

> Réalisé, mais par une autre voie que les pistes ci-dessous : terrain réel de la
> vallée d'Ossau (lac d'Artouste, pic du Midi d'Ossau) issu de l'IGN Géoplateforme
> (relief RGE ALTI, texture BD ORTHO), via le script hors-ligne
> `tools/fetch_terrain.py` qui produit `assets/terrain/{heightmap.png, ortho.jpg,
> terrain.txt}`. Au runtime, `stb_image` charge le tout (sans GDAL), un maillage
> unique est drapé de l'orthophoto, les altitudes servent au contact sol. Détails
> et comparaison des autres pistes dans `docs/TERRAIN.md`. Les pistes notées plus
> bas (Copernicus DEM, GDAL, LOD) restent des évolutions possibles.

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

### Changement de terrain (menu ou configuration)

- [x] Permettre de choisir le terrain par le fichier de configuration. Chaque
  terrain est rangé dans son sous-dossier `assets/terrain/<nom>/` (`heightmap.png`,
  `ortho.jpg`, `terrain.txt`, `landmarks.txt`) ; la clé `terrain` de
  `assets/config.txt` (ou la variable d'environnement `ARTOUSTE_TERRAIN`) choisit
  lequel charger au lancement. Le script `tools/fetch_terrain.py` est paramétré par
  zone (dictionnaire `ZONES`, nom passé en argument). Terrain livré : `ossau`
  (montagne). Zone décrite et prête à générer : `cote-landes` (côte basco-landaise,
  Bayonne -> Vieux-Boucau) -- lancer `tools/fetch_terrain.py cote-landes`.

    Reste possible plus tard : un menu en jeu et le changement de terrain à chaud
    (sans relancer). Le moteur recharge déjà tout au démarrage ; un changement à
    chaud demanderait de reconstruire le terrain et de replacer l'appareil.

### Bâtiments 3D

- [x] Bâtiments en volume sur les terrains de plaine (ville côtière). Emprises au
  sol de la BD TOPO de l'IGN (WFS), extrudées à leur hauteur réelle (murs + toit
  plat), murs clairs et toits tuile. Outil hors-ligne `tools/fetch_buildings.py`
  (filtre les constructions < 2 m) -> `assets/terrain/<zone>/buildings.bin` ;
  rendu par `render::Buildings` en un maillage statique unique, éclairé et noyé
  dans la même brume que le terrain. Côte basco-landaise : ~156 000 bâtiments.

    Pistes plus tard : variété des toits (plat/2 pentes selon la nature BD TOPO),
  niveaux de détail (LOD) pour alléger les grandes villes, bâtiments sur Ossau.

### Manuel

- [ ] Fournir un PDF propre dans les artéfacts Linux et Windows (.tar.gz et .zip) issu du README.md afin de guider l'utilisateur sur le fonctionnement.

## Quelques observations

- [ ] FUEL_BURN_MAX_LPH = 194.0f : nos fiches indiquent 155 kg/h à puissance maxi. Avec kérosène à 0,8 kg/L, cela donne environ 194 L/h. La conversion est juste.

- [ ] MASS = 1100.0f : les fiches indiquent 895 kg à vide. 1 100 kg correspond à une masse en charge raisonnable, cohérent avec un pilote + carburant.

- [ ] LEVEL_GAIN = 6000.0f : c'est le rappel artificiel vers l'horizontale mentionné dans les constantes. Il est honnêtement documenté comme une aide non réaliste. À réduire progressivement quand le pilotage sera maîtrisé.

- [ ] HudMode::Overlay avec "instruments ronds verts superposés (Super HUD)" : ce mode n'est pas dans nos fiches. C'est une bonne idée, à documenter dans PANEL.md.

- [ ] forceRunning() dans Turbine : pratique pour les tests, à garder en debug uniquement, à ne pas exposer en jeu final.
