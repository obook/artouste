# ROADMAP

## Super HUD

- [x] Superposer à l'image des instruments à définir en vert transparent uniquement pour ne par géner la vue afin de permettre un pilotage réaliste, la liste des instruments est à rechercher dans docs/tableau-de-bord, l'implémentation se fait un par un, raccourci clavier/manette cyclique hud au 4 coins, hud superposé, pas de hud.

Liste des instruments par priorité : voir Priorité 1 du fichier PANEL.md

## Réalisme

- [ ] Le tableau de bord est celui qui est standard FlightGear, le changer avec celui qui est préparé dans assets/models/Alouette-II-panel. Attention : il manque encore le panel.ac (fond du tableau) attendu par le chargeur (Interior/Panel/panel.ac), et les objets renommés dans les .ac (compas_fond, collectif_aiguille, triple_aiguilles, fuel_fond) imposent de mettre à jour les listes de sous-meshes de src/render/LoadedHelicopterInstruments.cpp ; voir le Read-Me.txt du dossier.

- [x] Étudier la possibilité de recevoir des messages radio (pré-enregistrés, synthèse vocale) : fait, messages de la tour en synthèse vocale (Flite) avec sous-titre à l'écran

- [X] Ajouter les livées Armée de l'Air, Armée de Terre (ALAT), Marine nationale, Sécurité civile

- [x] Vérifier la livrée du rotor principal, il semble qu'il n'y a aucune, donc faire en gris foncé

### Mode assisté conservé d'une carte à l'autre

- [x] Le mode assisté restait allumé au démarrage d'une carte alors qu'il doit
  repartir éteint. `resetToStart` appelait bien `m_assist.reset()`, mais celui-ci
  ne fait que resynchroniser les commandes lissées et laissait l'interrupteur
  intact. Corrigé par un `FlightAssist::disable()` appelé au départ en vol depuis
  le menu (`applyMenuSession`), et non dans `resetToStart` qui sert aussi aux
  touches `R` et `X` en vol, où couper l'assistance surprendrait le pilote.

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

- [x] Varier la clairance de décollage : six formulations tirées au sort dans `ApplicationRotorRadio.cpp`, sans jamais répéter celle du décollage précédent, avec salutation calée sur l'heure du cycle jour/nuit (`good morning`, `good afternoon`, `good evening`). Durée du sous-titre proportionnelle à la longueur du message.

### Séquence radio ATC complète

Aller au-delà de la simple clairance : un échange en plusieurs temps, conforme à la phraséologie OACI (Doc 9432), du contact initial au changement de fréquence, plus des phrases d'ambiance en vol.

**Séquence visée**

1. Contact initial du pilote au plein régime (`request take-off`).
2. Clairance de la tour, 3 à 5 s plus tard (déjà en place, avec ses variantes).
3. Readback du pilote, obligatoire en phraséologie OACI.
4. Compte rendu `airborne` en montée, puis `frequency change approved` de la tour.
5. Phrases d'ambiance espacées en vol (trafic signalé, QNH, message à toutes stations).

**Points à trancher avant de commencer**

- **Nom du terrain.** Des fichiers son figés diraient toujours le même terrain, alors que le nom vient de `helipads.txt` et change sur les neuf cartes. Piste retenue : garder la synthèse à l'exécution pour les phrases qui nomment la tour, et ne figer en WAV que les phrases neutres (readback, changement de fréquence, ambiance).
- **Indicatif.** Le message actuel dit `Fox-Bravo`. Une immatriculation Gendarmerie (F-MJGN, indicatif `Gendarmerie Hotel November`) serait plus juste pour une Alouette II, mais il faudra la reprendre partout d'un coup.
- **Verrou de rotor.** Le rotor reste bloqué jusqu'à la fin de l'annonce (`setRotorHold`). Avec trois échanges avant le décollage, l'attente au pad passerait à une dizaine de secondes : libérer plutôt dès la fin de la clairance, et laisser le readback se jouer pendant la montée en régime.
- **Filtre radio.** `radioize()` applique déjà passe-bande, saturation, squelch et roger beep. Des WAV filtrés en amont devraient donc emprunter un autre chemin de lecture, comme les sons de combat, sous peine d'être filtrés deux fois.
- **Voix.** Le jeu embarque la voix clustergen `cmu_us_slt` compilée dans le binaire. Toute phrase pré-enregistrée doit sortir de la même voix, sinon la tour change de timbre en cours d'échange.

**Ce qu'il ne faut pas annoncer.** Le modèle de vol ignore le vent et le ciel est vide : une clairance qui annoncerait un vent établi, un trafic en approche ou une attente mentirait au pilote. S'en tenir à ce qui est simulé, ou simuler d'abord.

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

### Habiller les bâtiments extrudés selon leur région

Les emprises et les hauteurs viennent de la BD TOPO, donc la SILHOUETTE des
villes est juste. Ce qui l'est moins, c'est l'habillage : une seule façade et une
seule palette de toits servent partout, et elles ont été dessinées pour la côte
basque et les Landes. Sur Paris, cela donne une ville aux toits de tuile rouge,
ce que trois secondes de survol suffisent à démentir. Deux chantiers distincts,
qui peuvent se mener séparément.

- [ ] **Façades haussmanniennes.** `assets/textures/facade.png`, produite par
  `tools/facade/generer_facade.py`, est une tuile générique de 12 x 6 m : trois
  travées de 4 m sur deux étages de 3 m, enduit clair grainé, fenêtres
  rectangulaires à croisillon, bandeau de plancher, pilastres discrets. Elle est
  répétée sur tous les murs de toutes les cartes.

    Le vrai obstacle n'est pas le dessin mais la structure. Une façade
  haussmannienne n'est pas un motif répétable, c'est une composition sur toute la
  hauteur : rez-de-chaussée commerçant, entresol, deuxième étage noble à balcon
  filant, troisième et quatrième plus sobres, cinquième à nouveau à balcon
  filant, corniche, puis comble en zinc. Une tuile qui se répète tous les deux
  étages ignore où elle se trouve dans la hauteur de l'immeuble et ne peut donc
  pas rendre ce rythme.

    Deux voies, par ordre de coût :

    1. Ne changer que le dessin, sans toucher au moteur. Pierre de taille beige
       à joints horizontaux marqués, fenêtres plus hautes et mieux
       proportionnées, garde-corps en fonte à chaque étage. On n'aura pas les
       balcons filants aux bons niveaux, mais le grain parisien plutôt que le
       grain de pavillon, pour le prix d'un script réécrit.
    2. Atlas en trois bandes verticales (socle, étage courant répétable,
       couronnement) et sélection de la bande dans `building.frag` selon la
       hauteur du fragment dans le mur. L'information nécessaire est déjà là :
       `BuildingsMesh` calcule les UV verticaux en mètres réels depuis le sol du
       bâtiment (`vTop = height / FACADE_TILE_H_M`). Il manque la logique de
       choix et une texture en trois parties. C'est la seule voie qui rende
       vraiment l'immeuble parisien.

    Dans les deux cas, prévoir un jeu de textures PAR RÉGION plutôt qu'une seule
  pour tout le dépôt, sur le principe des options par carte : une carte des
  Landes ne doit pas hériter des façades de Paris.

- [ ] **Toits de zinc.** `ROOF_PALETTE` (`src/render/buildings/BuildingsMesh.cpp`)
  compte six teintes dont trois identiques de tuile terre cuite, pesée pour
  dominer à 50 %, plus une tuile chaude, une tuile patinée et une ardoise. Le
  commentaire du code l'assume : "couleurs régionales (côte basque et Landes)".
  Une teinte est tirée par bâtiment, de façon stable mais SANS AUCUN RAPPORT avec
  la photo qui se trouve juste dessous : un immeuble couvert de zinc gris se
  retrouve coiffé de tuile rouge.

    Paris, c'est le zinc. Bleu-gris clair quand il est neuf, virant au gris terne
  en patinant, avec de l'ardoise plus sombre et plus bleue sur les édifices
  anciens, et de la tuile qui ne reparaît qu'en périphérie. S'y ajoute une
  difficulté de forme, pas seulement de couleur : le toit parisien est un comble
  brisé (mansarde), là où l'extrusion pose un toit plat. La couleur seule ne fera
  donc pas tout, mais elle corrigera l'essentiel de ce qui saute aux yeux.

    Trois façons de s'y prendre :

    1. Palette par carte, déclarée en donnée (clé d'`options.txt` ou fichier
       dédié) : `toits zinc` pour Paris, `toits tuile` ailleurs. Simple, dans la
       ligne des autres options par carte, et corrige immédiatement le défaut le
       plus visible.
    2. Teinter chaque toit d'après l'ORTHOPHOTO, en y lisant la couleur au centre
       de l'emprise. Juste partout et sans donnée à saisir, puisque la photo
       montre précisément le toit vu de dessus. `BuildingsMesh` charge déjà
       l'orthophoto côté processeur pour le filtre des cabanes sur l'eau, il n'y
       aurait qu'à l'étendre à toutes les cartes. Risque : reprendre la couleur
       brute donnerait des toits ternes et salis d'ombres, il faudra sans doute
       raviver la saturation.
    3. Compromis : lire l'orthophoto mais s'aligner sur la teinte la plus proche
       dans une palette propre. On garde des toits nets tout en devenant
       régionalement juste.

    La voie 1 est celle qui rapporte le plus vite, la voie 2 celle qui a le plus
  d'avenir, et rien n'empêche de faire la première en attendant la seconde.

### Monuments de Paris en 3D

- [~] Poser les monuments parisiens en volume sur la carte `paris`, à partir des
  35 modèles d'helijah (Emmanuel Baranger, scène FlightGear "Paris V2", 2009),
  le même auteur que l'Alouette II du projet. Source :
  `http://embaranger.free.fr/flightgear/scenery/ParisV2/ParisV2.htm`, archive
  `ParisV2-25-06-2009.tar.gz` (24,5 Mo, à récupérer un cran plus haut :
  `http://embaranger.free.fr/flightgear/scenery/ParisV2-25-06-2009.tar.gz`).
  Attention, l'adresse `helijah.free.fr/flightgear/scenery/` renvoie 404 : la
  page des scènes est sur `embaranger.free.fr`, seul le hangar des appareils
  est resté sur `helijah.free.fr`. Copie de secours si le lien tombe : dépôt
  GitHub `FGMEMBERS-TERRAGIT/e000n40-objects`, sous-dossier `e002n48/`.
  L'archive contient un `.ac` et sa texture par monument
  (`Models/Region-Paris/`) et les positions dans `Scenery/Objects/e000n40/`.

    Fait le 28/07/2026 : le mécanisme est en place, quatre monuments sont posés
  (tour Eiffel, Arc de Triomphe, Sacré-Coeur, Hôtel des Invalides, ce dernier en
  deux morceaux et imparfait, voir le point 9).
  Un `monuments.txt` facultatif par carte
  (`lon lat altitude cap echelle_h echelle_v rayon_m fichier nom`, l'altitude
  acceptant le mot-clé `sol`), lu par `Terrain::loadMonuments` dans
  `render::Monument` ; l'exclusion des emprises BD TOPO sous un monument dans
  `BuildingsMesh` ; un shader `monument.vert/frag` qui ajoute au rendu des
  modèles le test alpha (sans lui, les ajours du treillis restent opaques) et la
  brume du terrain. Le chargement recentre chaque modèle sur sa boîte
  englobante, sans quoi la coordonnée du fichier ne voudrait rien dire de commun
  d'un monument à l'autre : les auteurs de scène ne posent pas tous l'origine au
  milieu de leur géométrie, celle du Sacré-Coeur en est à 70 m. Restent les 31
  autres monuments.

    MÉTHODE, tirée des quatre premiers. Le centre et le cap se mesurent sur les
  emprises BD TOPO, qui sont vectorielles et exactes : cap par la direction
  dominante des côtés pondérée par leur longueur, modulo 90, et non par le côté
  le plus long, qu'une emprise très subdivisée rend trompeur. L'ÉCHELLE, elle,
  ne se mesure PAS sur BD TOPO, qui simplifie les grands monuments en blocs
  pleins sans percer les cours : la relever sur l'orthophoto, tuiles de détail
  chargées, en rendant deux fois la même vue au nadir avec et sans le monument
  et en comparant les silhouettes. Aucune échelle commune ne se dégage d'un
  monument à l'autre (1,029, 0,951, 1,0 et 1,0 en plan pour les quatre premiers) :
  chacun se mesure.

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

    Le mécanisme de pose d'un modèle ponctuel, qui manquait, sert aussi aux
  phares de Capbreton ci-dessus : `monuments.txt` n'a rien de parisien, toute
  carte peut en poser un.

    Coordonnées. Les positions exactes sont dans les fichiers `.stg` de
  l'archive, une ligne `OBJECT_STATIC fichier.ac lon lat altitude cap` par
  monument. Attention à l'ordre des champs : longitude d'abord, latitude
  ensuite. À défaut d'archive, les coordonnées WGS84 se retrouvent par la
  méthode IGN plus OSM plus Wikipédia déjà appliquée aux autres cartes.

    Points durs identifiés :

    1. RÉGLÉ. Doublon avec les bâtiments. `buildings.bin` extrude déjà les
       emprises BD TOPO de la Tour Eiffel, du Louvre ou de Notre-Dame ; poser le
       modèle par-dessus donnait deux géométries imbriquées. `BuildingsMesh`
       écarte maintenant les emprises sous un monument, dans le rayon que
       déclare sa ligne (`rayon_m`), plus un test point-dans-polygone pour la
       grande emprise qui l'englobe sans que son centre en soit proche. Quatre
       emprises écartées sous la tour Eiffel, ses quatre volumes empilés.
    2. RÉGLÉ. Calage vertical. Le relief de Paris fait 1024 x 1024 pour 18 017 m
       sur 9 685 m, soit environ 17,6 m par cellule, trop lâche pour poser
       proprement un socle. Le champ altitude accepte donc une valeur explicite,
       le mot-clé `sol` rendant la main à `heightAt` là où le relief suffit
       (c'est le cas du Champ de Mars, plat).
    3. RÉGLÉ. Orientation. Les `.ac` de la scène arrivent déjà en Y vers le haut,
       Assimp ne les réoriente pas : `fgToAssimp` ne concerne que les décalages
       des fichiers d'assemblage FlightGear, pas la géométrie. Le cap du fichier
       est un cap boussole appliqué en rotation autour de Y, cap 0 laissant le
       modèle tel que son auteur l'a orienté.
    4. RÉGLÉ, mais à surveiller. Échelle. Les modèles FlightGear sont en mètres,
       et pourtant celui de la tour Eiffel n'est pas aux proportions de
       l'édifice : 121,5 m de côté pour 268 m de haut, contre 125 et 300, ses
       quatre planchers tombant à 52, 105, 240 et 268 m au lieu de 58, 116, 276
       et 300. Régulièrement tassé, donc, pas amputé de son sommet. D'où deux
       facteurs par monument, horizontal et vertical, et non un seul. À vérifier
       monument par monument : rien ne dit que les autres soient justes.
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
    9. Hôtel des Invalides posé mais IMPARFAIT, à reprendre. Le modèle place
       mal ses éléments les uns par rapport aux autres : aucune pose ne satisfait
       à la fois le corps du complexe et l'église du Dôme, décalés d'une
       quinzaine de mètres l'un de l'autre dans le fichier. Il est donc posé au
       COMPROMIS, chacun fautif de 7 m.

       Une découpe en deux a été essayée puis ABANDONNÉE : la boîte de découpe
       tranchait dans le bâti et laissait une façade décollée du côté du dôme.
       Découper par une boîte ne marche que sur une pièce franchement détachée,
       comme le Panthéon au milieu de ses immeubles voisins ; pas sur une pièce
       soudée au reste. Si l'on veut vraiment séparer les deux ici, il faudra
       passer par Blender et couper au bon endroit, pas par un rectangle.

       Rien ne dit que les autres modèles soient exempts du même défaut de
       placement interne : le vérifier avant de croire à un mauvais réglage de
       pose.

    10. Panthéon posé mais IMPARFAIT, accepté en l'état. Le cap (18,5) et la
        position sont mesurés, mais la superposition à l'orthophoto reste
        approximative. Un balayage conjoint échelle et décalage donne un optimum
        vers 0,95 en plan plutôt que le 1,0 posé : à essayer si l'on y revient.
    11. Notre-Dame posée mais IMPARFAITE, acceptée en l'état. Le calage sur
        l'orthophoto y est moins sûr qu'ailleurs pour une raison qui tient à la
        photo : la prise de vue IGN de cette carte montre la cathédrale EN
        CHANTIER (entre 2020 et 2023, toiture déposée, grues, parvis en
        travaux), donc les contours sur lesquels s'appuie le calage sont
        brouillés et le maximum du balayage est large (0,87 à 0,93 se valent).
        Le modèle, lui, représente la cathédrale intacte, ce qui correspond de
        nouveau à la réalité depuis la réouverture de décembre 2024 : c'est la
        photo qui est périmée. À reprendre le jour où la carte sera refabriquée
        sur une ortho plus récente. Voir aussi le point suivant sur l'âge de
        cette orthophoto, qui ne concerne pas que Notre-Dame.
    12. Âge de l'orthophoto de la carte paris. Le chantier de Notre-Dame la date
        d'entre 2020 et 2023. D'autres travaux parisiens de cette période y
        figurent donc, et le même écart entre la photo et un modèle 3D actuel se
        reposera ailleurs. Vérifier si la Géoplateforme propose une prise de vue
        plus récente ; la refabrication imposerait de retuiler la carte.
    13. Centre Pompidou : bande noire sur la façade est, non résolue. Sont
        écartés : l'élimination des faces arrière (réactivée, bande identique),
        la moitié nuit de l'atlas (tous les UV sont dans la moitié jour, même
        après répétition), un objet sans texture (il n'en reste qu'un) et la
        zone noire de l'atlas (un carré de 64 px qu'aucune des 6 222 faces ne
        recouvre). Le rendu est pourtant du noir pur, donc un texel noir est
        bien lu quelque part. Deux défauts du même modèle ont en revanche été
        corrigés : le cap était retourné de 180 deg, et une couche coplanaire
        parasite (objet "jour1", 252 sommets, entièrement contenue dans le
        corps) provoquait un scintillement ; elle a été retirée du fichier.
    14. Front de Seine (Beaugrenelle) : pose non résolue, reprise en cours.
        Le modèle a d'abord été trouvé dans la Seine, cap à -109,5 alors qu'il
        fallait environ 15 : la ligne de tours barrait le fleuve. Reposé au
        centre des vingt-quatre tours de plus de 45 m relevées dans BD TOPO
        (2,284600 / 48,850166) et au cap de leur axe principal (42,8 deg, soit
        15,4 pour ce modèle), il tombe à peu près sur la rive gauche.

        Reste un désaccord non tranché, relevé sur une paire de repères tracés
        à l'écran sur un même bord de bâtiment : ce bord est à 439 m du centre
        du modèle et devrait être à 559. Une rotation conservant les distances,
        il manque 120 m, et UNE SEULE paire de repères ne permet pas de savoir
        s'il faut translater le modèle ou l'agrandir de 1,27. La translation a
        été essayée et renvoie les tours dans le fleuve, donc c'est
        vraisemblablement l'échelle ou le point d'ancrage qui est en cause.

        Ce qui rend le diagnostic incertain : l'étendue du modèle (884 m) colle
        pourtant à celle des tours relevées (918 m), ce qui contredit un facteur
        de 1,27. L'hypothèse la plus probable est que le centre de la boîte
        englobante du modèle ne correspond pas au centre des tours réelles.

        Marche à suivre : relever DEUX paires de repères sur deux bâtiments
        éloignés, ce qui détermine exactement rotation, échelle et translation.
        Si les tours du modèle ne correspondent à rien de reconnaissable dans la
        photo, leur espacement interne est faux comme aux Invalides : écarter
        alors ce modèle et laisser l'extrusion BD TOPO, juste par construction.

    15. Antennes de la tour Eiffel, à modéliser sous Blender. Le modèle s'arrête
       au sommet de la structure : après mise à l'échelle il culmine à 300 m,
       alors que la tour atteint 330 m depuis que les antennes de radiodiffusion
       la coiffent (1957, rehaussées depuis, 330 m après celle de la TNT en
       2022). Ce n'est pas un facteur d'échelle qui manque, ce sont trente
       mètres de géométrie absente : une aiguille à créer, à greffer sur le
       campanile et à texturer dans le même atlas. À faire dans un `.ac` séparé
       posé à la même coordonnée plutôt que dans `TourEiffel-ba.ac` : tant que la
       licence du modèle d'origine n'est pas éclaircie (voir plus bas), mieux
       vaut ne pas modifier le fichier d'un tiers, et un second monument dans
       `monuments.txt` ne coûte qu'une ligne. Sans urgence : la silhouette est
       déjà juste pour tout le reste.

    Licence : GPL v2, comme le projet. L'archive d'origine ne l'annonçait nulle
  part et son `Read-Me.txt` se bornait à créditer Bertrand AUGRAS, auteur des
  scènes X-Plane dont les modèles sont tirés
  (`http://baugras.club.fr/xplane/Site/france.html`). Emmanuel Baranger a
  confirmé la licence par courriel le 28/07/2026. L'attribution figure dans
  `CREDITS.md` et en bas du `README.md`.


#### Monuments posés, TOUS à revérifier en vol

Trente-deux modèles sont en place. Aucun n'est considéré comme validé : la carte
a trop bougé depuis les premières poses. Le chargement ramène désormais le point
le plus bas du modèle au sol et non son origine, ce qui a déplacé verticalement
tout ce qui avait un socle décalé ; les rayons de dégagement ont été resserrés
d'un bloc ; et vingt modèles sont passés par une chaîne automatique sans jamais
avoir été regardés en vol.

Trois points à contrôler sur chacun :

1. Le CAP. La mesure ne le donne que modulo 180 : une pose sur deux peut être
   retournée bout pour bout. Le Centre Pompidou l'était, repéré parce que son
   escalator se trouvait à l'est au lieu de l'ouest. Chercher un détail
   dissymétrique et connu : façade principale, clocher, cour d'honneur.
2. L'ÉCHELLE. Ne pas se fier aux emprises BD TOPO, qui simplifient les grands
   monuments en blocs pleins sans percer les cours, ni aux cotes publiées, dont
   le point de départ est souvent incertain. Comparer à l'orthophoto, tuiles de
   détail chargées, et se méfier de la parallaxe au-delà d'une vingtaine de
   mètres de hauteur.
3. L'ASSISE ET LA POSITION. Le monument doit toucher le sol et se superposer à
   son emprise photographiée.

- [x] Tour Eiffel
- [ ] Arc de Triomphe
- [ ] Sacré-Coeur
- [ ] Hôtel des Invalides -- imparfait par construction, voir le point 9
- [ ] Panthéon -- imparfait, voir le point 10
- [ ] Notre-Dame de Paris -- imparfait, voir le point 11
- [ ] Opéra Garnier
- [ ] Église de la Madeleine
- [ ] Grande Arche de la Défense
- [ ] Tour Montparnasse
- [ ] Hôtel de Ville
- [ ] Palais du Louvre
- [ ] Bibliothèque nationale de France
- [ ] Centre Pompidou -- bande noire non résolue, voir le point 13
- [ ] Église Saint-Eustache
- [ ] Église Saint-Sulpice
- [ ] Sainte-Chapelle
- [ ] Tour Saint-Jacques
- [ ] Hôtel Concorde Lafayette
- [ ] Grand Palais
- [ ] Palais de Chaillot
- [ ] Palais du Luxembourg
- [ ] Assemblée nationale
- [ ] École militaire
- [ ] Palais omnisports de Bercy
- [ ] Campus de Jussieu
- [ ] Front de Seine -- EN COURS, voir le point 14
- [x] Maison de la Radio
- [ ] Quartier de la Défense
- [ ] Opéra Bastille
- [ ] Place Vendôme
- [ ] Place de la Concorde

À vérifier aussi, mais qui ne concerne pas un monument en particulier :

- [ ] Rayons de dégagement. Ils écartent 830 emprises BD TOPO sous les
  monuments. Trop petits, des immeubles percent le modèle ; trop grands, ils
  effacent du bâti réel que le modèle ne remplace pas. La première série en
  écartait 2 087 et rasait des quartiers entiers autour de La Défense et du
  Front de Seine.

Écartés, avec la raison :

- [ ] Île de la Cité -- le modèle contient Notre-Dame et la Sainte-Chapelle,
  déjà posées séparément : doublon garanti. À reprendre seulement si l'on
  renonce aux deux poses individuelles.
- [ ] Champ de Mars -- pure dalle de sol de 1 268 x 398 m, sans rien au-dessus
  de 8 m : elle recouvrirait l'orthophoto des jardins sans rien apporter.
- [ ] Ministère du Travail -- le modèle se trouve à 2,2 km de l'endroit que son
  nom désigne. Identifier ce qu'il représente avant de le poser.
- [ ] Invalidessol -- quadrilatère de 402 x 941 m qui plaque une photo de
  l'esplanade au sol, doublon de notre orthophoto IGN.

### Piste à explorer : le dépôt FlightGear de Benoît Laniel

- [ ] Faire le tour de `http://blaniel.free.fr/pub/flightgear/`, repéré depuis la
  page des scènes d'helijah. Inventaire au 28/07/2026 :

    - `pyrenees/pyrenees.tar.bz2` (5,7 Mo). Le seul dossier qui touche au
      territoire du projet, donc le premier à ouvrir.
    - `paris/paris_photo.7z` (54,5 Mo) et `paris_dds.7z` (49,5 Mo). Le sol
      photographique de Paris dont parle la page ParisV2, converti en DDS.
      Vraisemblablement sans intérêt ici : notre orthophoto IGN fait déjà 3,6 m
      par pixel sur l'ensemble de la carte, et 0,25 m sur la fenêtre de détail
      tuilée.
    - `brest/brest_photo.7z` (6,8 Mo), 6 m par pixel, hors de nos cartes.
    - `corine/`, `water/`, `fgimport/`, `osgdem_terrain.7z` (41,7 Mo),
      `find_elevation.cxx`. Outillage de fabrication de scènes plutôt que
      données : à regarder pour la méthode, pas pour le contenu.

    ATTENTION à la licence avant d'espérer quoi que ce soit. Le `README.txt` de
  Brest annonce du Creative Commons Attribution NonCommercial ShareAlike 2.0.
  La clause non commerciale est incompatible avec la GPL v2 du projet : ces
  données ne pourraient pas voyager dans l'archive d'une version, même si
  Artouste ne se vend pas. Vérifier la licence dossier par dossier, elle n'est
  pas forcément la même partout.

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
  (1 arbre / espacement^2). Un budget global (~1,6 M, clé `arbres_max` de config.txt ou
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

## Mode zombie

### Largueur (boss) et yeux lumineux

- [x] Une manche sur cinq est désormais une manche de boss (`WaveManager::isBossWave`,
  `BOSS_WAVE_INTERVAL`). Un largueur (`ZombieHorde::Type::Brood`) y apparaît dès
  l'ouverture, escorté de la moitié seulement des marcheurs habituels, puis lâche
  un marcheur toutes les trois secondes autour de lui tant qu'il tient debout. La
  manche ne peut donc pas se gagner en patientant : l'anti-blocage de 90 secondes est
  suspendu tant qu'il vit. Il encaisse cinq roquettes (5000 PV contre 100 pour un
  marcheur), avance à 45 % de la vitesse d'un marcheur, et son modèle comme sa sphère
  de collision sont agrandis 3,2 fois, soit près de six mètres de haut (un seul champ
  `scale` pour les deux, afin que la silhouette et la cible ne divergent pas).
  Neutralisé : 500 points, annonce `LARGUEUR NEUTRALISÉ !` et jauge de vie au HUD
  tout le combat. Le râle `rale.wav`, jusqu'ici inutilisé,
  annonce son apparition (non spatial comme l'annonce de vague : il apparaît à
  quelques centaines de mètres, un son spatialisé y serait inaudible).

- [x] Yeux lumineux : deux billboards additifs par zombie (`render::combat::ZombieEyes`,
  `assets/shaders/zombie_eyes.*`), verts pour un marcheur et rouges pour un largueur,
  ce qui signale le boss avant même qu'on distingue sa silhouette. Ils sont calés sur la
  matrice d'instance du corps (`ZombieHorde::buildEyes`), pas sur l'os du cou animé par
  le squelette : à distance de jeu, l'écart ne se voit pas, et cela évite de poser une
  pose par instance. La lueur s'éteint pendant l'animation de chute, et grossit avec
  la distance pour rester repérable depuis l'hélicoptère.

    Ce grossissement a demandé deux essais. Viser une taille apparente constante
  (rayon proportionnel à la distance) rendait les yeux parfaitement lisibles de loin,
  mais donnait à moyenne portée des lueurs d'un mètre de large, bien plus grosses que
  la tête qui les porte : des boules vertes flottantes plutôt que des yeux. La loi
  retenue croît en racine de la distance, plafonnée à cinq fois le rayon de près
  (atteint vers 200 m) : la taille apparente diminue toujours quand on s'éloigne, mais
  moins vite que la perspective, et la lueur reste solidaire de la silhouette. Le
  plafond a été arrêté à mi-chemin entre les deux essais, la première loi donnant des
  lueurs trop grosses et la deuxième trop discrètes : à 100 m, le rayon vaut environ
  46 cm, contre 60 et 38 cm pour les deux essais.

### Sons de combat et sphère de collision de l'appareil

- [x] Deux défauts liés, signalés en jeu : le bruit d'impact d'un pneu sur l'appareil
  ne s'entendait pas, et le bruit de lancer saturait à courte distance. Le lancer
  utilise un échantillon de sept secondes alors que le geste est instantané : une horde
  qui lance en rafale empilait une dizaine de queues de son, dont la somme saturait la
  sortie et masquait le reste. Les instances simultanées d'un même son sont désormais
  plafonnées (trois en général, deux pour le lancer), le lancer est passé de 0,6 à 0,35
  de volume, et l'impact de 0,8 à plein volume, son échantillon étant le plus discret
  du lot (une quinzaine de décibels sous les autres) alors qu'il porte l'information la
  plus utile au joueur.

    Seconde cause pour l'impact : la sphère de collision de l'appareil ne faisait que
  2,5 m de rayon, soit la bulle de la cabine, sur une Alouette II qui mesure près de dix
  mètres poutre de queue comprise. Un pneu qui passait visiblement dans la machine la
  traversait sans rien déclencher, ni dégâts ni bruit. Portée à 4 m.

### Traces d'impact des roquettes

- [x] Chaque impact laisse désormais une trace de forme et de taille propres, au lieu
  du même rond de 3,5 m pour tous. La FORME vient de l'angle d'arrivée : une roquette
  qui tombe à la verticale creuse un rond, une roquette rasante étire sa tache le long
  de sa trajectoire (orientée par `ScorchView::yaw`, et décalée vers l'avant, la gerbe
  partant devant le point d'impact). La TAILLE vient de la portée du tir : +50 % de
  rayon à 150 m, plafonné à 5 m. L'ellipse conserve sa surface (grand axe multiplié par
  la racine de l'allongement, petit axe divisé par elle), sans quoi une trace allongée
  serait aussi une trace démesurée.

    La loi physique exacte pour la forme (rapport 1/sin de l'incidence, la tache d'un
  cône incliné) a été écartée : elle diverge à l'horizontale et, avec un canon fixe et
  un nez à peine piqué, l'incidence d'arrivée reste souvent sous 20 degrés, si bien que
  presque tous les tirs auraient saturé le plafond et que toutes les traces se seraient
  de nouveau ressemblées. On garde la tendance sur une interpolation bornée, qui étale
  les cas de jeu entre le rond et l'allongement maximal.

    Rien ne change côté jeu : la zone létale reste `EXPLOSION_RADIUS_M`, la trace est
  un décalque. La boule de feu, elle, garde son rayon fixe (elle pourrait suivre la même
  logique).

    Pistes pour aller plus loin : types de zombies (coureur, colosse, cracheur) plutôt
  qu'une horde uniforme ; plafond toxique qui monte avec les manches ; ravitaillement
  en munitions à récupérer en se posant ; manche bonus entre deux vagues.

### Yeux qui flottaient devant le visage

- [x] Défaut signalé en jeu : les lueurs vertes ne tenaient pas sur la tête. Elles
  étaient posées à un point fixe du repère du modèle (1,62 m de haut, 11 cm en avant),
  au motif qu'à distance de jeu l'écart avec l'os du cou ne se verrait pas. Mesure faite
  sur le pack : le crâne s'écarte de ce point de 13 à 34 cm selon la variante et
  l'instant du cycle, dérive de root motion déjà compensée, et il se déplace lui-même
  dans une boule de 9 à 29 cm de rayon pendant la marche. Un crâne mesurant 26 cm, aucun
  point fixe ne pouvait convenir : les neuf variantes du pack ne partagent même pas la
  même position de tête au repos (jusqu'à 27 cm d'écart entre elles).

    Les yeux sont donc calibrés au chargement sur l'os qui pilote le haut de la tête,
  variante par variante (`SkinnedModel::eyePoints`) : on repère cet os par vote des
  sommets de la tranche haute, puis on exprime les deux yeux DANS son repère. Le rendu
  les relit sur la pose qu'il vient de dessiner (`SkinnedZombies::eyeAnchors`), au prix
  de deux produits matrice-point par lot déjà posé. Vérifié sur les neuf variantes et
  vingt-quatre instants du cycle : les ancrages restent tous dans la boîte du crâne.

    Deux essais en vol ont ensuite recalé le placement, qui partait de la boîte du
  crâne. "Sur les oreilles et trop gros de près" : cette boîte fait 18 à 29 cm de large
  (oreilles et cheveux compris), si bien qu'un demi-écart pris sur elle visait les
  oreilles, alors que l'écart entre pupilles vaut 6,4 cm quelle que soit la coiffure ;
  et le rayon de base de 13 cm donnait une boule de 26 cm sur une tête de 26. Le repère
  devient donc le NEZ, point le plus avancé du crâne, qui donne l'axe du visage, la
  hauteur et l'avancée ; le rayon tombe à 3,2 cm, la croissance à distance étant
  relancée (référence à 1 m, plafond à 15) pour garder le loin lisible : 32 cm de rayon
  à 100 m contre 46 avant, mais quatre fois moins au contact.

    "Sur le front" : le point le plus avancé n'est pas toujours le nez -- sur trois
  variantes c'est le front ou une mèche, et le regard remontait. On retient désormais la
  plus basse des deux estimations, celle tirée du nez et celle tirée de la taille de la
  tête, bornée entre 11 et 18 cm sous le sommet du crâne. Mesuré après correction : 11,0
  à 15,6 cm selon la variante, contre 8,0 à 13,6 avant.

    Retenu en l'état après essai en vol : juste sur la plupart des variantes, approximatif
  sur quelques-unes. Le pack ne donne pas de repère d'yeux, et ses neuf têtes ne partagent
  ni la même proportion ni le même point le plus avancé ; aller plus loin demanderait un
  ancrage saisi à la main, variante par variante.

    La horde ne fournit plus que la couleur et le rayon (`buildEyeTints`), dans le même
  ordre que les matrices et les `kind` : elle ignore où le squelette a posé la tête, ce
  qui n'est pas son affaire.

### Pas d'annonce de la tour en mode zombie

- [x] Le mode zombie n'annonce plus rien : on entre en combat turbine et rotor déjà au
  régime, face à une horde, et une autorisation de décollage n'y a pas sa place. Le
  verrou de rotor qui accompagnait l'annonce saute avec elle, sans quoi l'appareil
  resterait cloué au pad en attendant une réplique qui ne vient plus.

    Deux effets de bord relevés en chemin, corrigés dans la foulée pour le vol libre.
  D'abord la voix lisait "Dax-Seyresse (pad est) tower" : le nom d'hélipad porte une
  précision entre parenthèses que la synthèse prononce telle quelle (Dax et Paris sont
  concernés). Elle est retirée comme l'était déjà le préfixe "Aérodrome de" -- la tour
  annonce le terrain, pas le pad. Ensuite l'annonce ne se réarmait qu'en voyant la
  turbine redescendre sous la moitié du régime : deux vols lancés d'affilée turbine
  chaude, et seul le premier était annoncé. `applyMenuSession` réarme désormais
  explicitement (`resetRadioMessage`), sous-titre compris.

### Le largueur emporte ce qu'il a lâché

- [x] Abattre le largueur fait éclater sur place tous les marcheurs qu'il a lâchés :
  une boule de feu et un cri par marcheur, et ils comptent comme des mises à mort, le
  joueur les ayant gagnées en abattant le boss. Les marcheurs venus du bord de l'arène,
  eux, continuent leur chemin. La manche de boss cesse ainsi de traîner : ce n'était plus
  qu'un ménage de fin, sans enjeu, une fois le largueur tombé.

    Ces marcheurs rapportent 25 points CHACUN, et non le barème du kill multiple utilisé
  pour le souffle d'une roquette : celui-ci plafonne à trois têtes, ce qui convenait à une
  explosion mais pas ici, où le largueur peut en avoir lâché quinze. Un largueur abattu
  vaut donc 500 points de prime, plus la roquette qui l'achève, plus 25 par marcheur
  emporté.

    Un marcheur porte donc désormais son origine (`ZombieHorde::Zombie::fromBrood`, posé
  par `spawnBroodling`), et ses yeux sont ROUGES comme ceux du largueur au lieu de verts.
  La couleur prévient : tout ce qui luit rouge tombera avec le boss. On distingue le
  largueur de sa portée à la taille de la lueur, trois fois plus large à son échelle.

    Le `RocketSystem` accepte pour l'occasion une détonation qui ne vient d'aucun tir
  (`addExplosion`) : purement visuelle, sans dégâts de zone ni trace au sol, puisque ce
  n'est pas un impact de roquette.

### Carburant perdu au contact du sol

- [x] En mode zombie, toucher le sol fend le réservoir : rien en deçà de 3 m/s (un
  posé), puis 2 litres par (m/s) d'excès AU CARRÉ. Le carré plutôt qu'une droite,
  qui faisait fuir trop de kérosène à chaque contact : la fuite suit l'énergie du
  choc, si bien qu'une touche un peu ferme se paie en minutes de vol (8 L à 5 m/s,
  50 L à 8 m/s, 98 L à 10 m/s) alors qu'un vrai crash vide les 575 L du réservoir
  (578 L à 20 m/s) et cloue l'appareil au sol, turbine éteinte. Le bruit d'impact
  des boulettes (`toxic_impact.wav`) accompagne le choc, à la position de
  l'appareil comme les autres coups reçus.

    La vie, elle, ne se perd que face aux zombies. Le contact au sol l'entamait
  dans une première version, à rebours de ce qui était voulu ; il ne touche plus
  qu'au carburant, et ne peut donc plus terminer une partie d'un coup. La sanction
  du crash n'est plus la mort mais l'immobilisation.

    Le seuil est calé sur ce que le joueur PEUT voir : la jauge affiche des litres
  entiers, donc une fuite de moins d'un demi-litre ne bougerait rien à l'écran. En
  dessous, on ne joue même pas le bruit du choc, un son sans effet visible se
  lisant comme un bug. Le premier vrai choc tombe ainsi à 3,5 m/s.

    La vitesse ne peut se mesurer QUE dans la physique : le contact annule aussitôt
  la composante verticale, si bien que la boucle de jeu, bien plus lente que la
  simulation à pas fixe, ne verrait plus qu'un appareil posé, vitesse nulle.
  `FlightModel` relève donc la vitesse complète au pas qui ENTRE en contact (rentrer
  dans un versant à l'horizontale reste un contact) et la tient à disposition
  jusqu'à lecture (`consumeGroundImpact`). Rester posé ne produit aucun nouveau
  contact, et repositionner l'appareil oublie une valeur non lue, sans quoi la
  partie suivante paierait un posé qui n'a pas eu lieu. Le combat décide du prix
  (`CombatMode::applyGroundImpact` rend des litres), la physique tient le réservoir
  (`FlightModel::drainFuel`).

### Panne sèche

- [x] Réservoir à zéro : la turbine s'éteint, le rotor descend et s'arrête, comme
  n'importe quelle extinction. Une minute environ sépare la panne de l'arrêt
  complet, et un redémarrage tenté à sec s'amorce puis se coupe au pas suivant : il
  ne reste qu'à quitter le vol.

    Le test de panne vivait DANS la branche de consommation, gardée par
  "carburant > 0". Il ne voyait donc que le réservoir vidé par la turbine
  elle-même. Un réservoir tombé à zéro autrement, ce que fait désormais un choc au
  sol qui le fend, laissait la turbine tourner indéfiniment à sec : mesuré à
  turbine 1,00 et rotor 1,00 vingt secondes après la fuite. La vérification est
  sortie de la branche et se fait à chaque pas.

    Le voyant CARB s'éteignait au même instant : comme tous les cadrans, il suivait
  "turbine arrêtée = planche hors tension", si bien que la panne sèche effaçait sa
  propre explication. Il reste désormais ROUGE réservoir vide, quel que soit l'état
  de la turbine, et la ligne du HUD passe de "BAS" à "PANNE". C'est la seule alarme
  qui survit à l'extinction, et les deux modes d'affichage (quatre coins et
  superposé) en profitent, tous deux passant par `alarmeCarb`.

### Démarrage refusé sur fond de réservoir

- [x] Appuyer sur le démarreur avec moins de 2 litres (`FUEL_START_MIN_L`) ne fait
  plus rien : ni séquence, ni son. La séquence dure une bonne minute et brûle près
  de deux litres avant que le rotor ne prenne son régime ; en dessous, la turbine
  s'éteignait en pleine montée, après avoir fait tout son bruit pour rien, et
  l'appareil ne décollait pas.

    Le cas le plus traître n'était pas le réservoir vide, coupé dès le premier pas
  de simulation, mais le fond de réservoir : la jauge affichant des litres entiers,
  "0 L" peut cacher un demi-litre, assez pour amorcer un démarrage voué à mourir.
  Mesuré à 0,30 L : turbine à 0,63 après trente secondes, puis extinction.

    Le garde-fou vit dans `FlightModel::toggleTurbine`, qui refuse un démarrage
  mais accepte toujours une coupure ; la touche `T` et le bouton `Start` y passent
  tous deux, au lieu d'appeler `Turbine::toggle` directement.

### Silence à la fin de partie

- [x] La fin de partie fige le vol (`frozen` dans `mainLoop`) mais laissait tourner le
  son : turbine, rotor et radio continuaient derrière le bandeau, sur un appareil
  abattu. La fin de partie suit désormais le même chemin que la pause
  (`AudioEngine::setPaused`), qui garde la position des boucles : la partie suivante les
  reprend là où elles s'étaient tues.

    Les sons ponctuels du combat ne passent pas par là (ils ne sont pas des boucles) et
  leur purge s'arrête avec `update()`, faute d'appel une fois le jeu figé : un râle ou
  une queue d'explosion se serait poursuivi seul. D'où `stopCombatSounds`, qui les coupe
  et les libère, appelé à chaque image de fin de partie plutôt que sur le front de
  `gameOver` (sans effet une fois la liste vide, et rien à retenir entre deux images).

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

- [x] Nuit deux fois plus rapide que le jour (clé `lune_vitesse` de `config.txt`,
  défaut 2) : la vitesse du temps de `soleil_vitesse` est multipliée par ce facteur
  entre le coucher (18 h) et le lever (6 h). Avec les valeurs par défaut, un cycle
  complet dure un quart d'heure, dix minutes de jour et cinq de nuit. Le calcul est
  sorti de l'Application (`src/app/CycleJourNuit.cpp`) : fonction pure, sans fenêtre
  ni contexte graphique, donc vérifiable -- huit cas couvrent la durée du cycle, la
  continuité au coucher et au lever, le départ de nuit, le temps figé, la marche
  arrière et un facteur absurde.

- [x] Configuration personnelle entretenue toute seule (`src/app/Config.cpp`), pour
  qu'un `config.txt` écrit par une version ancienne ne se périme jamais :
  - **option nouvelle** ajoutée à la fin du fichier avec sa documentation, valeur
    du modèle ; les réglages existants ne sont jamais réécrits ;
  - **option renommée** renommée sur place, valeur et mise en page conservées, via
    la table `clesRenommees()` (une entrée ne s'en retire jamais). Réécriture par
    fichier intermédiaire puis remplacement, pour ne jamais laisser une
    configuration tronquée ;
  - **modèle effacé ou abîmé** réécrit depuis la copie embarquée dans l'exécutable
    (`ConfigModele.hpp`, fabriquée par CMake depuis `assets/config.default.txt`) ;
    un modèle valide mais adapté volontairement n'est jamais touché.
  Les tests verrouillent la cohérence entre les trois listes (clés du chargeur, du
  modèle et de la table de renommage) : ajouter une option oblige à toucher les
  trois, sinon ils tombent.

- [x] Recherche de mise à jour au lancement (clé `verifier_maj` de `config.txt`,
  activée par défaut, coupée par `ARTOUSTE_NO_MAJ`). Le tag de la dernière release
  est demandé à l'API de GitHub dans un fil séparé (`src/app/MiseAJour.cpp`), donc
  sans jamais retarder la fenêtre ni le vol ; s'il est plus récent que
  `ARTOUSTE_VERSION_SEMVER` (le champ `VERSION` du projet), le menu de démarrage
  l'annonce et propose la page <https://obook.github.io/artouste/>, ouverte par le
  bouton `Télécharger` ou la touche `M`. La page affiche elle aussi ce numéro,
  mais sans rien demander à personne : il est écrit en clair dans `docs/index.html`
  (liens de classe `release-tag` et champ `softwareVersion`), remplacé à chaque
  release par le job `page` de `.github/workflows/release.yml`, qui commite le tag
  sur `main` -- ce qui reconstruit la page. Sans libcurl à la
  compilation, la vérification n'a pas lieu, comme la radio. Reste à faire : rien
  de bloquant ; à surveiller,
  le quota anonyme de l'API GitHub (60 requêtes par heure et par adresse IP), qui
  ne gêne qu'un réseau derrière lequel beaucoup de joueurs partageraient la même
  sortie -- l'échec est alors silencieux, sans conséquence pour le vol.

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
