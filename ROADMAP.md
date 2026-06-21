# ROADMAP

## Plateforme

- [ ] Étudier une version pour Microsoft Windows

## Super HUD

- [x] Superposer à l'image des instruments à définir en vert transparent uniquement pour ne par géner la vue afin de permettre un pilotage réaliste, la liste des instruments est à rechercher dans docs/tableau-de-bord, l'implémentation se fait un par un, raccourci clavier/manette cyclique hud au 4 coins, hud superposé, pas de hud.

Liste des instruments par priorité : voir Priorité 1 du fichier PANEL.md

- [x] Mettre l'altitude dans le même style que la boussole, mais à gauche et verticalement

## Réalisme

- [ ] Configuration

    Module de chargement de la configuration

    Quelques éléments seront configurable par un fichier .json, chargé et appliqué au lancement, il est éditable à la main, la première variable est la position de la caméra intérieur

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

## Quelques observations

- [ ] FUEL_BURN_MAX_LPH = 194.0f : nos fiches indiquent 155 kg/h à puissance maxi. Avec kérosène à 0,8 kg/L, cela donne environ 194 L/h. La conversion est juste.

- [ ] MASS = 1100.0f : les fiches indiquent 895 kg à vide. 1 100 kg correspond à une masse en charge raisonnable, cohérent avec un pilote + carburant.

- [ ] LEVEL_GAIN = 6000.0f : c'est le rappel artificiel vers l'horizontale mentionné dans les constantes. Il est honnêtement documenté comme une aide non réaliste. À réduire progressivement quand le pilotage sera maîtrisé.

- [ ] HudMode::Overlay avec "instruments ronds verts superposés (Super HUD)" : ce mode n'est pas dans nos fiches. C'est une bonne idée, à documenter dans PANEL.md.

- [ ] forceRunning() dans Turbine : pratique pour les tests, à garder en debug uniquement, à ne pas exposer en jeu final.
