# ROADMAP

## Super HUD

- [x] Superposer à l'image des instruments à définir en vert transparent uniquement pour ne par géner la vue afin de permettre un pilotage réaliste, la liste des instruments est à rechercher dans docs/tableau-de-bord, l'implémentation se fait un par un, raccourci clavier/manette cyclique hud au 4 coins, hud superposé, pas de hud.

Liste des instruments par priorité : voir Priorité 1 du fichier PANEL.md

## Réalisme

- [ ] Le tableau de bord est celui qui est standard FlightGear, le changer avec celui qui est préparé dans assets/models/Alouette-II-panel. Attention : il manque encore le panel.ac (fond du tableau) attendu par le chargeur (Interior/Panel/panel.ac), et les objets renommés dans les .ac (compas_fond, collectif_aiguille, triple_aiguilles, fuel_fond) imposent de mettre à jour les listes de sous-meshes de src/render/LoadedHelicopterInstruments.cpp ; voir le Read-Me.txt du dossier.

- [x] Étudier la possibilité de recevoir des messages radio (pré-enregistrés, synthèse vocale) : fait, messages de la tour en synthèse vocale (Flite) avec sous-titre à l'écran

- [X] Ajouter les livées Armée de l'Air, Armée de Terre (ALAT), Marine nationale, Sécurité civile

- [x] Vérifier la livrée du rotor principal, il semble qu'il n'y a aucune, donc faire en gris foncé

### Lacet en approche (dérive à droite)

- [ ] Dérive en lacet à droite en approche finale (réduction du collectif) :
  diagnostic et plan de correction détaillés dans APPROACH_YAW.md (fiche
  hors dépôt, avec CLAUDE.md). Cause : `REACTIVE_TORQUE * (collective -
  COLL_HOVER)` (`FlightModel.cpp`) devient négatif sous le collectif de vol
  stationnaire, ce qui pousse le nez à droite -- physiquement correct sur
  l'Alouette II, mais peut-être trop marqué pour un pilote sans expérience.
  Correction prévue en deux volets indépendants : atténuer ce couple
  uniquement côté réduction de collectif (`REACTIVE_TORQUE_DESCENT_FACTOR`
  dans `constants.hpp`, laisse la montée intacte) et, en mode assisté,
  ajouter un terme de compensation sur l'écart absolu au collectif de vol
  stationnaire (`ASSIST_STEADY_ANTITORQUE_GAIN` dans `FlightAssist.cpp`),
  pour couvrir aussi une descente stabilisée et pas seulement la variation
  du collectif.

### HAPI (aide à l'approche)

- [x] Balise HAPI (Helicopter Approach Path Indicator, voir
  `media/gt_installation_hapi.pdf`, guide DGAC/STAC janvier 2017) sur le pad de
  départ de chaque carte (d'abord l'aérodrome de Dax-Seyresse seul, puis étendu
  aux six autres cartes). Contrairement à l'optique réelle (4 faisceaux colorés visibles
  chacun depuis une plage d'angle différente), le simulateur calcule à chaque
  image l'angle d'élévation entre la balise et l'appareil du joueur (seul point
  de vue à simuler) et n'affiche qu'une seule lueur, dans la couleur du secteur
  correspondant -- fidèle au réel, où l'on ne voit jamais qu'une couleur à la
  fois. Seuils repris du guide (§ 4.4) : vert clignotant (trop haut), vert fixe
  (sur la pente, +/- 22'30"), rouge fixe (légèrement trop bas, 15' de plus),
  rouge clignotant (trop bas). Données par carte dans un fichier optionnel
  `hapi.txt` (`lon lat azimut_deg pente_pct nom`), généré par
  `tools/fetch_terrain.py` (clé `hapi` de `zones.py`) comme les hélipads et les
  lieux remarquables ; rendu dans `Application::drawHapi`
  (`src/app/ApplicationGround.cpp`).

    Calage Dax provisoire, faute de relevé d'obstacles réel : azimut 70°
  (aligné sur la piste bitumée 07/25 de Dax-Seyresse, approche vers l'est QFU
  07), pente 6 % (valeur usuelle pour une hélistation). Position identique à
  l'hélipad existant, à affiner sur place (méthode IGN/OSM habituelle).

    Pistes pour consolider : ouverture azimutale du faisceau (~10 % de
  divergence dans le guide) non simulée, la balise reste visible dans son
  secteur vertical quel que soit l'azimut d'approche ; taille de la lueur
  exagérée (rayon fixe en mètres) faute de billboard à taille d'écran
  constante, invisible au-delà de quelques centaines de mètres ; calage réel
  de Dax (azimut, pente, position exacte du HAPI derrière le point cible) à
  vérifier sur place ; sur les six autres cartes, faute de piste ou de DZ
  réelle recensée, l'azimut vaut simplement le cap de départ (`start_heading`)
  et la position est calée sur le pad plutôt que sur un relevé de terrain ;
  HAPI sur d'autres hélipads que celui de départ (hôpitaux avec hélistation
  dédiée).

### Sons

- [ ] Retravailler les sons, en ajoutant ceux d'Émmanuel.

- [ ] Au débranchement qu'un casque USB, le son ne revient pas à la sortie principale

- [x] Prévoir dans le fichier de configuration une URL pour un flux radio + commandes radio on/off et mixage Heli/Music (balance entre les deux) : fait, clé `radio_url` de config.txt, touche `K` et balance `-`/`+`

### Mode demo

**Route de la démo** (parcours du pilote automatique ; corriger l'ordre et les altitudes ici, c'est la référence) :

1. Décollage du pad (aérodrome de La Teste).
2. Survol de la Dune du Pilat à 1000 m (panorama).
3. Survol du cap Ferret en rase-mottes à 30 m.
4. Demi-tour et retour au pad, puis pose.

> Note : route courte volontairement, centrée sur les deux temps forts (panorama en altitude puis rase-mottes), pour une démo plus rythmée. Altitude = hauteur de vol (au-dessus du bassin et du littoral bas, proche de l'altitude mer). La Dune du Pilat (lat 44.5912130, lon -1.2020697) et le cap Ferret (lat 44.6184674, lon -1.2450709) sont des coordonnées explicites. Vitesse de croisière : 50 m/s (~180 km/h).

- [X] Mode démo : à l'atterrissage on voit 2 helipads proches, un a son texte ou l'hélico n'attérit pas et l'autre où l'hélico attérit n'en a pas (pas toujours reproductible)

- [X] Mode démo : décollage encore plus lent et doux, leger piqué avant pour avancer

- [X] On ne sait pas quand et de quelle manière on est sorti du mode démo, trouver une solution

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

### Point de départ et nouvelles cartes

- [X] `find_flat_start` (calage du point de départ) peut retenir la surface d'un lac,
  parfaitement plate, au lieu de la terre ferme. Contourné à la main pour la vallée
  d'Ossau (Fabrèges, au bord du lac) en recalculant `start_x`/`start_z` hors de l'eau.
  À corriger dans le pipeline : écarter les cellules d'eau (masque depuis l'orthophoto,
  ou seuil de couleur) avant de chercher le replat le plus plat.

- [ ] Variante étendue de la carte du Pic du Midi de Bigorre vers le nord pour inclure
  Bagnères-de-Bigorre (~14 km au nord) : agrandir `lat_max` (~43,08), soit une carte
  d'environ 26 km nord-sud, pour un poids en hausse d'environ 40 %.

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

### Fabriquer une carte neuve depuis le jeu

> Contexte, juillet 2026 : le gestionnaire de cartes du menu sait déjà fabriquer
> les TUILES d'une carte existante, en allant chercher la BD ORTHO à 0,25 m/px et
> en compressant en BC7 sur place. Il a besoin du `terrain.txt` de cette carte
> pour calculer sa grille : il améliore une carte, il n'en crée pas. Le socle
> (relief, orthophoto d'ensemble, calage) vient toujours des scripts Python de
> l'auteur. C'est ce chaînon qui manque, et c'est l'étape 4c de
> `docs/DISTRIBUTION.md`.

Une carte se définit par presque rien : un nom, une emprise, une finesse de
relief, une finesse d'ortho, un point de départ. Quatre morceaux à écrire pour
que le jeu sache la produire.

- [ ] **Relief par raster.** La Géoplateforme sert les altitudes en
  `image/x-bil;bits=32`, couche `ELEVATION.ELEVATIONGRIDCOVERAGE.HIGHRES`, soit un
  tableau brut de flottants : quelques requêtes au lieu des milliers que fait
  l'API JSON point par point. Même chemin libcurl que les tuiles, et pas même un
  décodeur d'image à brancher.

- [ ] **`heightmap.bin` lu par le moteur, prérequis.** Le moteur lit aujourd'hui
  `heightmap.png` en 16 bits (`stbi_load_16`), or stb_image_write ne produit que
  du 8 bits : un outil C++ ne peut pas écrire ce fichier. Lire un simple tableau
  d'altitudes lève l'obstacle, deux heures de travail.

- [ ] **Orthophoto d'ensemble.** Même service WMS que les tuiles, à finesse
  grossière, assemblée par blocs : `demanderBloc` existe déjà, il ne manque que
  l'écriture du JPEG.

- [ ] **Calage et écran.** Le `terrain.txt` est un pur calcul depuis l'emprise :
  largeur et hauteur en mètres, altitudes extrêmes lues dans le raster, taille de
  l'ortho, point de départ. Un panneau "Nouvelle carte" dans le gestionnaire, avec
  la même règle d'annonce que le reste : place occupée, volume à recevoir, durée.

Une carte ainsi fabriquée naîtra **nue** : ni lieux remarquables, ni hélisurfaces,
ni balise HAPI, ni bâtiments. Ce sont des données de repérage humain, vérifiées
une à une, et l'extrusion de la BD TOPO est un autre pipeline. Elle n'aura que son
point de départ.

- [ ] **Catalogue `assets/zones.txt`**, une ligne par zone (nom, emprise,
  finesses, titre). Il évite de saisir des coordonnées : le joueur choisit dans
  une liste, l'auteur enrichit le catalogue sans faire de release. Sans lui, la
  fabrication reste réservée à qui sait lire une emprise en degrés décimaux.

Estimation d'auteur, un à deux jours : une demi-journée pour le relief, deux
heures pour `heightmap.bin`, deux ou trois heures pour l'ortho, une demi-journée
pour le calage et l'écran, quelques heures d'essais réels sur une petite zone.

Le vrai risque n'est pas technique mais de civilité : une carte de montagne
demande déjà des centaines de requêtes pour ses tuiles, et le relief s'y ajoute.
L'outil doit s'espacer, réessayer proprement sur un refus 429, et ne jamais
paralléliser à outrance. Le service est public et gratuit.

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
  niveaux de détail (LOD) pour les bâtiments lointains, bâtiments sur Ossau.

- [x] Culling des bâtiments au rendu (champ de vue et brume). Le maillage unique est
  découpé en tuiles spatiales (grille d'environ 2 500 m), chacune dotée de sa boîte
  englobante et de sa plage d'indices. À chaque image, seules les tuiles présentes dans
  le champ de vue et en deçà de la brume sont dessinées (`Mesh::drawRange`). Le recalage
  d'origine s'annulant dans le produit final, le frustum s'extrait directement des
  coordonnées monde (`proj * vue`). Le gain est net en survol des grandes villes côtières :
  sur le bassin d'Arcachon, la charge GPU passe d'environ 95 % à 85 % et la cadence
  remonte vers 60 images par seconde. Les villes intérieures très denses comme Bordeaux,
  où toute l'agglomération tient dans le champ, restent bornées par la géométrie visible
  et appellent le LOD. Le diagnostic `ARTOUSTE_NO_CULL` désactive le culling pour mesurer
  son gain.

- [x] Mettre aussi les petits bâtiments pour toutes les cartes avec des montagnes ou peu de villes

- [ ] Placer les deux phares (feux d'entrée de port) de Capbreton, à l'embouchure
  du chenal du Boucarot : un vert au bout de l'estacade sud (environ
  `-1.4488, 43.6552`), un rouge au bout de la digue nord (environ
  `-1.4483, 43.6567`, d'après l'almanach nautique : "North Dike" Fl(2)R.6s,
  "South estacade" Fl(2)G.6s). Repérés après un signalement : un arbre
  poussait par erreur à l'emplacement de l'estacade sud, corrigé par une
  entrée `exclusions` dans `zones/cote_landes.py`, mais aucune structure n'y
  est encore dessinée.
  Candidats trouvés dans `fgdata` (dépôt GitLab `flightgear/fgdata`,
  `Models/Maritime/Misc/`) : `left_bank_beacon.ac` et `right_bank_beacon.ac`,
  exactement le couple de balises de rive gauche/droite qu'il faut ici.
  Contrairement aux bâtiments (BD TOPO, auto-extrudés) et aux hélipads (disque
  `assets/models/helipad/helipad.ac` déjà instancié), il n'existe aucun
  mécanisme pour poser un modèle ponctuel arbitraire à une coordonnée donnée :
  à construire, sur le modèle du chargement de l'hélipad (charger le `.ac`,
  instancier au lon/lat voulu, poser sur `heightAt`).

### Monuments de Paris en 3D

- [ ] Poser les monuments parisiens en volume sur la carte `paris`, à partir des
  35 modèles d'helijah (Emmanuel Baranger, scène FlightGear "Paris V2", 2009),
  le même auteur que l'Alouette II du projet. Source :
  `http://helijah.free.fr/flightgear/scenery/ParisV2/ParisV2.htm`, archive
  `Paris_V2.zip` (24,5 Mo). Copie de secours si le lien tombe : dépôt GitHub
  `FGMEMBERS-TERRAGIT/e000n40-objects`, sous-dossier `e002n48/`. L'archive
  contient un `.ac` et sa texture par monument (`Models/Region-Paris/`) et les
  positions dans `Scenery/Objects/e000n40/`.

    État des lieux. La carte `assets/terrain/paris/` existe déjà (heightmap,
  ortho, `buildings.bin` de 14 Mo, `helipads.txt`, `options.txt` avec `arbres 0`)
  et son `landmarks.txt` compte déjà 26 lieux, dont la plupart des monuments
  visés (Tour Eiffel, Notre-Dame, Arc de Triomphe, Louvre, Sacré-Coeur,
  Panthéon, Concorde, Invalides, Opéra Garnier, Bastille, Beaubourg,
  Montparnasse). Il ne reste donc que la géométrie à ajouter, pas les points de
  navigation ni le terrain.

    Aucune conversion de format à prévoir. `render::ModelLoader` charge déjà
  l'AC3D par Assimp (c'est le format d'`alouette.ac` et de tous les
  instruments) : les `.ac` d'helijah se chargent tels quels, sans passer par
  Blender ni par un export glTF. Seul point à vérifier du côté des fichiers :
  les textures. Si certains monuments arrivent en `.rgb` (format SGI), stb_image
  ne sait pas les lire et il faut les convertir en PNG.

    Mécanisme manquant, commun avec les phares de Capbreton ci-dessus : rien ne
  permet aujourd'hui de poser un modèle ponctuel arbitraire à une coordonnée
  donnée. À construire une seule fois pour les deux besoins, sous la forme d'un
  fichier optionnel par carte, par exemple `monuments.txt`
  (`lon lat altitude cap echelle fichier nom`), lu comme `landmarks.txt` et
  `hapi.txt` le sont par `Terrain::loadPlaces` et `Terrain::loadHapiUnits`, puis
  instancié au chargement sur le modèle de l'hélipad
  (`assets/models/helipad/helipad.ac`).

    Coordonnées. Les positions exactes sont dans les fichiers `.stg` de
  l'archive, une ligne `OBJECT_STATIC fichier.ac lon lat altitude cap` par
  monument. Attention à l'ordre des champs : longitude d'abord, latitude
  ensuite. À défaut d'archive, les coordonnées WGS84 se retrouvent par la
  méthode IGN plus OSM plus Wikipédia déjà appliquée aux autres cartes.

    Points durs identifiés :

    1. Doublon avec les bâtiments. `buildings.bin` extrude déjà les emprises
       BD TOPO de la Tour Eiffel, du Louvre ou de Notre-Dame ; poser le modèle
       par-dessus donnerait deux géométries imbriquées. Il faut un mécanisme
       d'exclusion d'emprises, sur le principe d'`exclusions.txt` qui ne sert
       aujourd'hui qu'à la végétation, mais appliqué à `BuildingsMesh`.
    2. Calage vertical. Le relief de Paris fait 1024 x 1024 pour 18 017 m sur
       9 685 m, soit environ 17,6 m par cellule, trop lâche pour poser
       proprement un socle. Prévoir une altitude explicite par monument dans le
       fichier plutôt que le seul `heightAt`.
    3. Orientation. Le cap des `.stg` suit la convention FlightGear et Assimp
       réoriente les `.ac` au chargement (voir `fgToAssimp` et le commentaire de
       `LoadedHelicopterInstruments.cpp`). La conversion vers la rotation Y du
       moteur est à écrire, puis à vérifier monument par monument.
    4. Échelle. Les modèles FlightGear sont en mètres, mais certains demandent
       un ajustement : garder un facteur d'échelle par monument dans le fichier.
    5. Culling et LOD. `render::Model` n'a ni découpage spatial ni niveau de
       détail, contrairement aux bâtiments. Trente-cinq modèles, c'est peu, mais
       ils seront dessinés en permanence.
    6. Résolution de l'ortho. 5001 x 2688 pixels pour 18 km font 3,6 m par
       pixel, donc des monuments nets sur un sol flou. La fenêtre de détail
       tuilée (`src/render/tuiles/`) devient presque obligatoire sur cette carte.
    7. Hors cadre. Le château de Versailles (2,120 / 48,805) tombe hors des
       bornes de la carte (lon 2,224 à 2,47 ; lat 48,815 à 48,902). Soit on
       l'abandonne, soit on élargit la carte au prix de la résolution. La
       Défense et l'héliport d'Issy-les-Moulineaux (LFPI) sont dans le cadre.
    8. Poids de l'archive. Environ 24,5 Mo de plus dans une release qui embarque
       déjà neuf cartes.

    Licence. Les modèles sont en GPL v2, comme le projet. Ajouter dans
  `CREDITS.md`, à côté de l'entrée Alouette II du même auteur : "Modèles 3D des
  monuments de Paris : helijah (Emmanuel Baranger), helijah.free.fr, scène
  FlightGear Paris V2, GPL v2".

### Végétation (arbres, forêts)

- [~] Prototype d'arbres en billboards instanciés (`render::Vegetation`). Chaque
  arbre est un billboard EN CROIX (deux quads verticaux perpendiculaires, fixes dans
  le monde, orientés par un azimut d'instance) : il garde du volume vu du dessus, là
  où un simple panneau face caméra s'amincissait en trait (inspiré de FlightGear). La
  géométrie de base (deux quads) est dessinée des milliers de fois par instanciation
  GPU, en puisant dans un atlas de 3 espèces (`assets/vegetation/trees_atlas.png` :
  sapin, feuillu, mélèze), l'espèce étant choisie par altitude (sapin et mélèze en
  montant, feuillu plus bas) et un tirage aléatoire. Transparence par ALPHA-TO-COVERAGE
  (sur le MSAA déjà actif) : bords de feuillage doux et tramés, pas un seuil net. Les positions sont semées à la volée au
  chargement du terrain d'après l'orthophoto : un arbre là où le sol est vert
  (signature de couleur de la forêt) et sous la limite forestière -- progressive :
  couvert plein jusque ~1900 m, raréfaction jusqu'à ~2200 m, rien au-dessus (évite la
  ligne de coupure nette et pose des pins épars sur les hautes pentes). Posé sur le
  relief. On écarte aussi les ZONES CLAIRES (grève, gravier, roche/neige, chemin) par
  un plafond de luminance. Deux masques à la résolution de l'ortho écartent des arbres :
  l'EAU (remplissage de proche en proche depuis chaque repère "Lac" de landmarks.txt --
  épouse la forme réelle du plan d'eau, un disque ratait le réservoir allongé de
  Fabrèges, et gère les lacs verts comme Pombie -- puis DILATÉ de ~22 m pour dégager le
  contour/la grève, de couleur trop proche de la forêt) et les BÂTIMENTS (emprises de
  buildings.bin rastérisées, sinon des arbres poussent sur les toits des villages).
  Même brume que le terrain / les bâtiments ;
  test alpha (pas de mélange), donc l'ordre de dessin n'importe pas. Chaque maille
  de la grille de semis donne au plus un arbre, donc l'espacement fixe la densité
  (1 arbre / espacement^2). Un budget global (~1,6 M, clé `tree_max` de config.txt ou
  la variable `ARTOUSTE_TREE_MAX`) éclaircit ensuite le semis de façon uniforme sur les
  grandes cartes très boisées (Bordeaux passe de ~4,9 M à ~1,6 M) pour limiter le
  surdessin des billboards croisés ; le baisser (500 000, voire 300 000) allège la charge
  sur une machine modeste. Activée par défaut : clé `arbres` de config.txt (`arbres 0`
  pour désactiver) ; `ARTOUSTE_NO_TREES` force la désactivation en priorité ;
  `ARTOUSTE_TREE_SPACING` règle la densité (défaut 8 m). En un seul appel de rendu
  instancié (plafond de sécurité à 6 M).
  Sprites PHOTOGRAPHIQUES : cellules extraites des atlas d'arbres de FlightGear
  (`assets/vegetation/fgdata-trees/`, GPL v2 -- voir CREDITS.txt), assemblées en
  `trees_atlas.png` par `tools/vegetation/compose_trees_atlas.py` (arbre calé sur la
  base). Un générateur procédural de repli reste dans `make_trees_atlas.py`.
  FONDU DE DENSITÉ à distance : loin de la caméra, seule une fraction des arbres est
  gardée (les autres rétrécissent vers leur base puis s'effacent), sur un rang stable
  par instance -- allège le remplissage au loin sans coupure nette ni scintillement.

    Pistes pour consolider : normal map (feuillage ombré PBR) et gestion des saisons
  (les atlas FlightGear ont 4 saisons) ; plus de variétés par espèce (FlightGear en a
  8/4) ; pipeline hors-ligne `tools/fetch_vegetation.py` (positions précalculées ->
  `vegetation.bin`, sur le modèle de `fetch_buildings.py`) au lieu du semis au
  chargement ; source de forêt plus fiable (BD Forêt IGN via WFS, ou OSM `landuse=forest`)
  que la seule couleur
  de l'ortho ; tri des prairies claires (qui attrapent encore quelques arbres) ;
  niveaux de détail et culling autour de l'appareil ; clé de densité par carte dans
  `terrain.txt` / `zones.py`.

### Ciel et nuages

- [~] Prototype de nuages en billboards (`render::Clouds`), sur le modèle de la
  végétation. Une couche de cumulus épars est semée au-dessus du relief (base déduite
  de l'altitude maximale du terrain) : chaque nuage est un amas de bouffées (sprites
  blancs face caméra) réparties dans un ellipsoïde à base plate qui se resserre vers le
  haut. Le volume vient de l'ombrage fait par le shader : base sombre et bleutée, sommet
  clair, atténué la nuit selon la hauteur du soleil ; fondu dans la brume au loin.
  Contrairement aux arbres (test alpha), les nuages demandent de la transparence par
  mélange avec tri de profondeur : les bouffées sont retriées de l'arrière vers l'avant
  à chaque image (`Clouds::draw`), dessinées sans écriture de profondeur. Sprite
  procédural (`tools/clouds/make_puff.py`). Sur Ossau : ~1500 bouffées.

    Pistes pour consolider : clé de config pour activer / désactiver (comme `arbres`) ;
  types de nuages (stratus, cirrus en couche 2D) ; déplacement lent avec le vent ;
  ombres portées au sol ; approche volumétrique (ray marching) pour un plus grand
  réalisme, bien plus lourde.

## Quelques observations à traiter

- [ ] FUEL_BURN_MAX_LPH = 194.0f : nos fiches indiquent 155 kg/h à puissance maxi. Avec kérosène à 0,8 kg/L, cela donne environ 194 L/h. La conversion est juste.

- [ ] MASS = 1100.0f : les fiches indiquent 895 kg à vide. 1 100 kg correspond à une masse en charge raisonnable, cohérent avec un pilote + carburant.

- [ ] LEVEL_GAIN = 6000.0f : c'est le rappel artificiel vers l'horizontale mentionné dans les constantes. Il est honnêtement documenté comme une aide non réaliste. À réduire progressivement quand le pilotage sera maîtrisé.

- [ ] HudMode::Overlay avec "instruments ronds verts superposés (Super HUD)" : ce mode n'est pas dans nos fiches. C'est une bonne idée, à documenter dans PANEL.md.

- [X] Mettre un PDF du README dans les archives des binaires dans les releases

### Sons

- [ ] pendant le début de montée de la rurbine, j'entends les pales => mettre le son des pales lorsque elles ont à 70% de 360 tr/min

### Manuel

- [X] Fournir un PDF propre et automatiquement à jour du readme dans les artéfacts Linux et Windows (.tar.gz et .zip) issu du README.md afin de guider l'utilisateur sur le fonctionnement.

## Interface

- [x] Menu de démarrage dans la fenêtre (ImGui), en remplacement de `launch.bat` :
  choix de la carte et du démarrage immédiat de la turbine, utilisable souris / clavier
  / manette. Voir `src/app/ApplicationMenu.cpp`. Sauté en capture, avec `ARTOUSTE_TERRAIN`
  ou `ARTOUSTE_NO_MENU`. `launch.bat` et `launch.sh` ont été retirés du dépôt.

- [x] Mode plein écran : lancement en plein écran (moniteur principal, résolution
  native), bascule fenêtré / plein écran par la touche `F`, curseur masqué en plein
  écran. Géométrie fenêtrée mémorisée et viewport/aspect réajustés (callback de resize).
  `ARTOUSTE_WINDOWED` force le fenêtré (dev). Non vérifiable sur la session Wayland de
  dev (le compositeur XWayland ne présente pas le plein écran GLFW) : à confirmer sous
  Windows.

- [x] Échap : en vol, retour au menu de démarrage (au lieu de quitter) ; dans le menu,
  quitte l'application. Boucle menu <-> vol dans `Application::run` + `applyMenuSession`.

- [ ] En plein écran, prévoir éventuellement une bascule true borderless-windowed sous
  Windows (au lieu du plein écran moniteur) si l'Alt-Tab ou le multi-écran pose souci.

- [ ] Double commande élève/instructeur (deux manettes en même temps) : aujourd'hui
  `Gamepad::activePad()` ne lit que le premier slot GLFW reconnu comme manette
  (`src/input/Gamepad.cpp:66-73`), la seconde manette branchée est totalement ignorée.
  Pour un pilotage à deux comme dans un vrai hélicoptère à double commande, il faudrait
  lire tous les slots connectés et fusionner les axes, par exemple en prenant pour
  chaque axe celui des deux qui s'écarte le plus du neutre (imite des commandes
  mécaniquement liées). Reste à trancher l'arbitrage des boutons à bascule (turbine,
  vue, HUD, menu, livrée, mode assisté) si les deux joueurs appuient en même temps :
  soit un seul pilote désigné a la main dessus, soit un bouton de prise de commande
  dédié à l'instructeur.

## Distribution Windows

- [x] Le Contrôle intelligent des applications (Smart App Control) de Windows 11
  bloque `launch.bat` : un script non signé qui lance un `.exe` non signé, sans
  bouton "Exécuter quand même". Résolu par la piste (2) : `launch.bat` a été
  retiré, son menu (choix de carte, turbine démarrée) est désormais intégré à
  l'exe lui-même (voir Interface, menu de démarrage ImGui). Reste en réserve la
  piste (3), signer l'exécutable (Authenticode, idéalement EV) pour gagner la
  confiance de SAC/SmartScreen.
