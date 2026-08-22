# Cartes : choix, configuration, régénération

Chaque terrain est rangé dans son propre sous-dossier de `assets/terrain/`, par
exemple `assets/terrain/ossau/` (vallée d'Ossau, montagne),
`assets/terrain/cote-landes/` (côte basco-landaise, de Bayonne à Vieux-Boucau),
`assets/terrain/arcachon/` (bassin d'Arcachon, du Cap Ferret à Marcheprime,
de Biscarrosse à Arès), `assets/terrain/cauterets/` (Cauterets - Gavarnie :
chemin des cascades, Pont d'Espagne, cirque de Gavarnie, montagne),
`assets/terrain/bordeaux/` (Bordeaux et son agglomération : la Garonne, l'aéroport
de Mérignac, Pessac, Cenon et Lormont), `assets/terrain/dax/` (centre-ville de
Dax et aérodrome de Seyresse : cathédrale, Pont Vieux, arènes, musée de l'ALAT,
Saint-Paul-lès-Dax, les Thermes, Seyresse et le Golf de Saint-Paul-lès-Dax --
recadrée sur la ville pour un sol net, voir ci-dessous),
`assets/terrain/bigorre/` (Pic du Midi de Bigorre : l'observatoire, le col du
Tourmalet, la station de La Mongie), `assets/terrain/paris/` (Paris intra-muros :
la tour Eiffel, Notre-Dame, Montmartre, le Bois de Boulogne et le Bois de
Vincennes), `assets/terrain/toulouse/` (Toulouse et l'aérodrome de Lasbordes :
le Capitole, Saint-Sernin, les Jacobins, la Garonne, la Cité de l'espace et
l'ancienne piste de Montaudran) et `assets/terrain/dax-arene/` (arène recadrée depuis `dax`, dédiée
au mode zombie -- voir la section dédiée ci-dessous, affichée au menu sous le
nom **Happy DeathHour**).
Un sous-dossier contient `terrain.txt` (calage), `heightmap.png` (relief),
`ortho.jpg` (orthophoto), `landmarks.txt` (lieux remarquables) et, facultatifs,
`helipads.txt` (hélipads à poser, par exemple un hôpital ou un port ; un par
ligne : `lon lat nom`), `hapi.txt` (balise HAPI sur le pad de départ, un par
ligne : `lon lat azimut_deg pente_pct nom` -- voir la section HAPI du README),
`buildings.bin` (bâtiments 3D), `bridges.bin` (ouvrages d'art, voir
ci-dessous), `forest.png` (masque de forêt, voir
ci-dessous), `zombies.txt` et `zombie_only.txt` (mode
zombie, voir ci-dessous). L'hélipad de la
zone de départ est toujours présent en plus de ceux de `helipads.txt`.

## Modifier la configuration

Le fichier `assets/config.txt` règle le lancement. C'est un simple fichier texte,
modifiable dans **n'importe quel éditeur**. Chaque ligne est une `clé valeur` ;
une ligne qui commence par `#` est un commentaire (ignoré).

> Rien de tout cela n'est obligatoire pour voler : **un PC et une manette de jeu
> suffisent**, l'archive extraite se lance telle quelle. Ces clés servent à régler
> le simulateur, pas à le faire fonctionner. Le clavier, lui, dépanne mais n'est pas
> recommandé : il ne donne que du tout ou rien là où un stick rend une valeur
> continue.

Ce fichier est ta configuration personnelle et n'est donc pas versionné. S'il est
absent au lancement, le simulateur le crée automatiquement en recopiant le modèle
`assets/config.default.txt` (lui, versionné), puis charge cette copie. Tu peux ainsi
modifier `assets/config.txt` à ta guise, ou le supprimer pour repartir des valeurs
par défaut.

Ton fichier suit ensuite les versions du jeu tout seul, sans que tu aies à le
refaire :

* **une option nouvelle** apparue dans une version plus récente est ajoutée à la
  fin de ton `config.txt`, avec les commentaires qui l'expliquent et sa valeur par
  défaut. Tes réglages ne sont jamais réécrits, on n'ajoute qu'à la fin ;
* **une option renommée** l'est aussi chez toi, en gardant la valeur que tu avais
  choisie et la place de la ligne. Sans cela, ton ancienne clé serait rejetée et
  ton réglage repasserait en silence au défaut ;
* **le modèle effacé ou abîmé** est réécrit à l'identique : le simulateur en porte
  une copie. Un modèle valide que tu aurais adapté volontairement, lui, n'est
  jamais touché.

Les clés disponibles :

* `terrain` : choisit la map chargée au démarrage (voir ci-dessous).
* `turbine_demarree` : `1` pour démarrer avec la **turbine et le rotor déjà au
  régime** (au lieu de la séquence de démarrage d'environ une minute), pratique
  pour décoller tout de suite en test ; `0` (défaut) pour un démarrage normal à
  froid. La variable d'environnement `ARTOUSTE_TURBINE_DEMARREE` a la priorité
  (`ARTOUSTE_TURBINE_DEMARREE=1 ./build/bin/artouste`).
* `demo` : `1` pour lancer le **mode démo automatique** au démarrage (vol joué tout
  seul, en boucle ; le terrain est alors forcé sur `arcachon`) ; `0` (défaut) sinon.
  Le bouton `Démo` du menu de démarrage (ou la touche `D`, ou le bouton `Y` de la
  manette) lance aussi la démo ; `Échap`, ou une action sur le manche, en sort. Depuis
  le menu, en sortir ramène au menu. Pendant la démo, la vue (`C` ou bouton `Y`), le
  HUD (`H` ou bouton `B`), le plein écran (`F`) et la radio (`K`, `-`/`+`) restent
  actifs sans l'interrompre. La variable d'environnement `ARTOUSTE_DEMO` a la priorité.
* `verifier_maj` : `1` (défaut) pour **rechercher une mise à jour** au lancement.
  Le simulateur demande, dans un fil séparé qui ne retarde ni la fenêtre ni le vol,
  le numéro de la dernière version publiée. S'il existe plus récent que la version
  installée, le menu de démarrage l'annonce en bas et propose d'aller la chercher
  sur <https://obook.github.io/artouste/> : bouton `Télécharger`, ou touche `M`, qui
  ouvre le navigateur du système. Rien n'est envoyé au passage : c'est une simple
  lecture, sans identifiant ni statistique, et un réseau absent ou lent ne change
  rien au lancement. `0` pour ne rien demander au réseau ; la variable
  d'environnement `ARTOUSTE_NO_MAJ` coupe aussi la recherche. Comme la radio, la
  vérification a besoin de libcurl à la compilation : sans elle, elle n'a
  simplement pas lieu.
* `radio_url` : URL d'un **flux radio internet** (MP3 sur HTTP) joué dans le
  cockpit, sous les sons moteur. Vide par défaut (pas de radio). La radio est
  **coupée au lancement** : la touche `K` l'allume puis la coupe en vol. La
  variable d'environnement `ARTOUSTE_RADIO_URL` a la priorité. La radio est une
  fonctionnalité optionnelle : sans libcurl à la compilation, URL vide ou réseau
  coupé, le simulateur reste silencieux sur ce point, sans erreur. Un voyant
  `RADIO` s'affiche dans le HUD tant que le flux joue, suivi de la part de la radio
  dans le mixage. Les touches `-` et `+` règlent la **balance radio/hélico** (un
  crossfade : monter la radio atténue d'autant le son de l'hélico, et inversement).
* `soleil_vitesse` : règle la **vitesse du temps** du cycle jour/nuit, c'est-à-dire
  la rapidité de la course du soleil. La durée réelle d'une journée complète vaut
  `86400 / soleil_vitesse` secondes. La valeur `72` (défaut) fait défiler une journée
  entière en vingt minutes ; `144` la réduit à dix minutes ; `1` correspond au temps
  réel, le soleil partant de l'heure locale du PC ; `0` fige le temps à 8h du matin (le
  soleil ne bouge plus). Pour toute valeur autre que `1`, le simulateur démarre à 8h, afin
  d'ouvrir sur une belle lumière. L'heure courante s'affiche dans le HUD, sur la ligne `HRE`
  du panneau supérieur droit.
* `lune_vitesse` : **multiplicateur de la vitesse du temps pendant la nuit**, du
  coucher (18 h) au lever (6 h). `2` (défaut) fait passer la nuit deux fois plus vite
  que le jour : voler une demi-journée dans le noir n'apporte pas grand-chose, alors
  qu'un ciel qui bascule du crépuscule à l'aube pendant qu'on rentre, si. `1` rétablit
  une nuit aussi longue que le jour, `4` l'expédie. Avec les valeurs par défaut
  (`soleil_vitesse 72`, `lune_vitesse 2`), un cycle complet dure un quart d'heure :
  dix minutes de jour et cinq de nuit. Sans effet quand le temps est figé
  (`soleil_vitesse 0`).
* `arbres_max` : **budget de végétation**, soit le nombre maximum d'arbres soumis à la
  carte graphique. Au-delà, le semis est éclairci uniformément. C'est le poste de
  rendu le plus lourd quand les arbres sont actifs (clé `arbres` ; `arbres 0` les
  coupe). La valeur par défaut est `1600000` ; sur une machine modeste ou une tablette,
  `500000` voire `300000` allègent nettement la charge, au prix d'une forêt plus
  clairsemée. La variable d'environnement `ARTOUSTE_TREE_MAX` a la priorité.
* `msaa` : **anti-crénelage**, le nombre d'échantillons par pixel. `4` (défaut) lisse
  bien les contours ; `2` réduit la bande passante mémoire de la carte graphique, pour
  une différence peu visible en 1080p ; `0` le désactive, ce qui est le plus rapide.
  Sur un GPU intégré qui peine ou une tablette qui chauffe, `2` puis `0` sont les
  premiers réglages à essayer. La variable d'environnement `ARTOUSTE_MSAA` a la priorité.

Par exemple, pour passer de la vallée d'Ossau à la côte landaise, ouvre
`assets/config.txt` et remplace :

```bash
terrain ossau
```

par :

```bash
terrain cote-landes
```

Enregistre, puis relance le simulateur : la nouvelle map est chargée. La valeur
doit être le nom exact d'un sous-dossier de `assets/terrain/` (ici `ossau`,
`cote-landes`, `arcachon`, `cauterets`, `bordeaux`, `dax`, `bigorre` ou `paris`).

Sans modifier le fichier, la variable d'environnement `ARTOUSTE_TERRAIN` a la
priorité, pratique pour essayer une map ponctuellement :

```bash
ARTOUSTE_TERRAIN=cote-landes ./build/bin/artouste
```

## Régénérer ou ajouter un terrain

Les terrains sont produits hors-ligne par `tools/fetch_terrain.py` (données IGN
Géoplateforme, Licence Ouverte Etalab 2.0). Le script prend le nom de la zone en
argument et écrit dans `assets/terrain/<zone>/` :

```bash
tools/.venv/bin/pip install Pillow numpy scipy   # une fois
tools/.venv/bin/python tools/fetch_terrain.py cote-landes
```

Pour ajouter une zone, copier une entrée du dictionnaire `ZONES` en tête du
script (bornes géographiques, mer ou montagne, point de départ, lieux
remarquables, hélipads). Voir [TERRAIN.md](TERRAIN.md) pour les détails du pipeline.

## Tuiles de détail (orthophoto fine)

Une carte est livrée avec une photo aérienne d'ensemble. Elle suffit en vol haut
et devient floue dès qu'on descend. Les tuiles sont la même photo en beaucoup
plus fin, découpée en carrés que le jeu charge au fil du vol. Une fois
installées, le sol redevient net au ras du sol. Elles ne changent rien d'autre :
ni relief, ni bâtiments, ni contenu de la carte.

Une carte sans tuiles est dite LR, une carte avec ses tuiles HR : c'est tout le
vocabulaire du gestionnaire de cartes, décrit plus bas.

L'orthophoto d'une carte est une seule texture : sa taille en mémoire vidéo
suit l'emprise du terrain, et c'est ce budget qui plafonne la finesse au sol.
Sur le bassin d'Arcachon, 35 x 49 km, l'ortho livrée est à 9,8 m/px : vu de
50 mètres, un pixel de photo couvre dix mètres de sol, le décor est une
bouillie. Descendre à 1,5 m/px sur cette carte demanderait 780 mégapixels, soit
780 Mo de mémoire vidéo même en BC7. Impossible.

Le jeu de tuiles lève la contrainte. La même orthophoto fine y est découpée en
carrés de 512 pixels, compressés en BC7 une fois pour toutes, et le moteur n'en
tient qu'une fenêtre autour de l'appareil (voir
`src/render/tuiles/Pyramide.hpp` et `Fenetre.hpp`). La mémoire vidéo devient
constante -- 89 Mo pour la fenêtre par défaut de 8192 px -- quelle que soit
l'emprise, et le lointain reste couvert par l'orthophoto d'ensemble, vers
laquelle le détail se fond progressivement.

### Produire un jeu de tuiles

`tools/terrain/fetch_tuiles.py` récupère l'orthophoto fine auprès de l'IGN et la
découpe, bloc par bloc, sans jamais tenir la mosaïque entière en mémoire :

```bash
cmake --build build --target orthotuiles          # une fois
cd tools
python3 -m terrain.fetch_tuiles ossau /media/disque/tuiles/ossau \
        --m-par-pixel 0.75 --reprendre --outil ../build/bin/orthotuiles
```

C'est long (une vingtaine de secondes par bloc de 9 x 9 tuiles, soit une demi-heure
pour une carte de montagne), d'où `--reprendre`, qui saute les blocs déjà faits.
Finesses raisonnables, choisies pour qu'un paquet de carte reste sous le
gigaoctet :

| Carte | Emprise | Ortho livrée | Tuiles | Disque | Zip |
|-------|---------|--------------|--------|--------|-----|
| ossau | 18 x 18 km | 3,6 m/px | 0,75 m/px | 713 Mo | 655 Mo |
| cauterets, bigorre | ~16 x 22 km | 3,8 - 4,9 m/px | 0,75 m/px | ~0,8 Go | |
| cote-landes | 16 x 28 km | 3,5 m/px | 0,75 m/px | ~1,1 Go | |
| arcachon | 36 x 49 km | 9,8 m/px | 1,5 m/px | ~1,0 Go | |
| bordeaux | 25 x 27 km | 5,3 m/px | 1,0 m/px | ~0,9 Go | |
| paris | 18 x 10 km | 3,6 m/px | 0,5 m/px | ~0,9 Go | |
| dax | 6 x 6 km | 0,85 m/px | 0,25 m/px | ~0,8 Go | |

Seule la ligne d'Ossau est mesurée ; les autres sont des estimations à
1,33 octet par pixel, la densité du BC7 avec ses niveaux de réduction. Le zip ne
gagne presque rien : des blocs compressés ne se recompressent pas.

L'IGN photographie la France à 0,20 m/px : viser plus fin ne rapporterait rien.

Sur les cartes de montagne, une part des tuiles tombe au-delà de la frontière
espagnole, hors couverture de la BD ORTHO : sur Ossau, 88 des 2209 tuiles n'ont
pas été écrites pour cette raison (2121 sur disque), et le moteur y garde
l'orthophoto d'ensemble.

Un jeu de tuiles n'est ni versionné ni publié : c'est de la donnée dérivée,
régénérable, sans commune mesure avec le dépôt. **Aucune release ne porte de
tuiles**, elles ne s'obtiennent que de l'IGN, par le gestionnaire de cartes du
jeu (décision du 28/07/2026, voir `docs/DISTRIBUTION.md`). À 0,25 m/px une seule
carte dépasse de toute façon le plafond de 2 Go d'un fichier de release.

Reste une cible d'empaquetage, pour son seul usage local : recopier un jeu d'une
machine à l'autre, ou l'archiver avant de refaire les tuiles à une autre finesse.

```bash
cmake -S . -B build -DARTOUSTE_TUILES_DIR=/media/disque/tuiles
cmake --build build --target tuiles     # écrit build/tuiles-<carte>.zip
```

### Où le jeu cherche les tuiles

Dans cet ordre, chaque racine étant essayée telle quelle puis avec un
sous-dossier `tuiles` :

1. `$ARTOUSTE_TUILES/<carte>/`, si la variable d'environnement est définie ;
2. `<tuiles_dossier>/<carte>/`, la clé de `assets/config.txt`, que le
   gestionnaire de cartes retient quand il fabrique ailleurs que dans le jeu ;
3. `assets/terrain/<carte>/tuiles/`.

Les deux premiers chemins existent pour les gros jeux qu'on préfère garder sur un
autre disque. Tout est facultatif : carte sans tuiles, dossier absent, tuile
manquante ou abîmée, pilote sans BC7, `tuiles_fenetre_px 0` dans la configuration
-- dans tous ces cas le terrain s'affiche avec sa seule orthophoto d'ensemble.

Un dossier ne compte comme jeu de tuiles que s'il porte un index : un dossier
laissé par une fabrication interrompue n'en a pas et sera ignoré.

Une tuile hors couverture BD ORTHO (blanc pur, au-delà de la frontière espagnole
sur les cartes de montagne) n'est volontairement PAS écrite : le moteur y
retombe sur l'orthophoto d'ensemble, qui a été recousue à la préparation de la
carte, plutôt que de plaquer un carré blanc sur le paysage.

## Le gestionnaire de cartes

Le menu de démarrage ouvre un écran des cartes, par le bouton `Cartes` ou par la
touche C. Il sert à trois choses : voir ce que chaque carte occupe, régler ce
qu'elle affiche, et passer une carte de LR à HR ou l'inverse. Il fabrique
lui-même les tuiles, sans passer par les scripts Python et sans rien installer.

Sa règle de conduite : **annoncer avant d'agir**. La place occupée par chaque
carte est affichée en permanence, celle qui reste sur le disque du jeu et sur
celui des tuiles aussi, et rien de lourd ni de destructeur ne part sans un
écran de confirmation chiffré.

### Ce que montre l'écran

Une ligne par carte, six colonnes :

| Colonne | Ce qu'elle dit |
|---------|----------------|
| Carte | le sous-dossier de `assets/terrain/` |
| État | LR, la carte livrée, sol flou au ras du sol ; HR, la même avec ses tuiles, sol net ; `HR (éteintes)` si elles sont là mais désactivées ; `HR (partiel)` si la fabrication a été interrompue |
| Socle | tout ce que pèse la carte hors tuiles : relief, orthophoto d'ensemble, lieux, hélisurfaces et bâtiments |
| Tuiles | ce que pèse le jeu de tuiles de détail ; `-` si la carte n'en a pas encore, `x` si elle n'a rien à en attendre |
| Bâtiments | oui ou non, un réglage et non un poids : les éteindre ne rend aucun octet ; `aucun` si la carte n'a pas de `buildings.bin` |
| Arbres | oui ou non : un réglage, jamais une taille, les arbres n'occupant aucun disque |

Sous la table, la carte choisie est détaillée : ce qu'elle hérite de la
configuration générale, où vivent ses tuiles, la finesse de sa photo d'ensemble
et celle de ses tuiles, avec le rapport des deux.

### Les touches

Le curseur de souris est masqué en plein écran : tout est atteignable au clavier,
les boutons ne sont qu'un raccourci.

| Touche | Effet |
|--------|-------|
| flèches | choisir la carte |
| Entrée | fabriquer les tuiles, puis confirmer |
| Suppr | supprimer les tuiles, puis confirmer |
| A, B, T | arbres, bâtiments, tuiles : allumer ou éteindre pour cette carte |
| R | rendre à cette carte les trois réglages généraux |
| Échap | annuler ce qui est en cours, sinon revenir au menu |

Les trois réglages sont écrits dans le `options.txt` de la carte, et seulement
ceux qu'on a explicitement pris : les autres continuent de suivre la
configuration générale, et l'écran dit lesquels.

### La finesse, choisie carte par carte

L'écran choisit seul la finesse des tuiles. Deux grandeurs entrent en jeu, et
elles ne servent pas à la même chose. Le RAPPORT entre la finesse des tuiles et
celle de l'orthophoto dit si la fabrication vaut le coup : des tuiles à 0,25 m/px
rendent Ossau, dont l'ortho est à 3,6 m/px, quatorze fois plus net, alors que les
mêmes n'apporteraient rien à dax-arene, déjà à 0,25. La finesse ELLE-MÊME, en
revanche, décide de ce que l'oeil voit à vingt mètres, et c'est une grandeur
absolue.

La règle appliquée (`interet`, dans `src/app/cartes/FabriqueTuiles.cpp`) :

- viser trois fois plus fin que l'orthophoto d'ensemble ;
- jamais plus fin que 0,20 m/px, la finesse de la source IGN ;
- jamais plus grossier que 0,25 m/px, soit 64 % de l'information que porte la
  source. C'est cette borne, et non le rapport, qui s'applique à presque toutes
  les cartes : leur tiers d'orthophoto tombe au-delà. Le prix est assumé, une
  carte de montagne pèse alors 7 Go et demande une heure et demie de
  fabrication, à quatre tuiles par seconde ;
- ne rien proposer du tout si le gain resterait sous 1,5, ce qui est le cas
  d'une petite carte découpée dans une image déjà fine, comme dax-arene. Le
  bouton est alors grisé et l'écran dit pourquoi.

Un jeu de tuiles qui n'est pas plus fin que l'orthophoto est écarté au chargement
par le moteur (`render::Terrain::ouvrirDetail`) : la carte est alors annoncée LR
malgré les mégaoctets posés sur le disque, et l'écran propose de les refaire ou
de les supprimer.

### Avant de télécharger

La confirmation donne quatre chiffres, tous calculés avant le moindre octet reçu :

- le nombre de tuiles et leur finesse ;
- la place occupée une fois la carte fabriquée, exacte, le BC7 ayant une densité
  fixe ;
- le volume à télécharger et une fourchette de durée, remplacée par une mesure
  dès le premier bloc reçu ;
- l'espace libre du disque visé, et ce qu'il en restera après.

La fabrication tourne en tâche de fond, avec sa barre d'avancement et son débit
mesuré. Elle s'arrête par Échap, et ce qui est écrit reste écrit : une reprise
saute les tuiles déjà là.

### Reprendre une fabrication interrompue

L'index d'un jeu de tuiles est écrit avant la première tuile, et il décrit la
grille VOULUE, pas celle qui est sur le disque. Un jeu interrompu ressemblerait
donc à un jeu complet, et l'écran annoncerait des tuiles qui ne couvrent qu'un
coin de la carte.

D'où un second fichier, `fabrication_inachevee.txt`, posé dans le dossier de
sortie en même temps que l'index et retiré à la seule condition que la grille
ait été parcourue en entier. Il survit à un arrêt demandé comme à une coupure de
courant. Tant qu'il est là :

- la carte est annoncée `HR (partiel)` ;
- l'écran donne le compte, par exemple "87 tuiles écrites sur 3066" ;
- le bouton devient `Reprendre la fabrication`, et la reprise garde la finesse du
  jeu entamé plutôt que celle que viserait une fabrication neuve : deux grilles
  différentes dans le même dossier ne se mélangent pas ;
- seules les tuiles manquantes sont téléchargées, l'écran le dit avant de partir.

`fetch_tuiles.py` pose et retire le même fichier, aux mêmes conditions : une
fabrication lancée en ligne de commande et interrompue s'annonce partielle elle
aussi. Les jeux fabriqués avant que le script ne le fasse n'en portent pas et
restent tenus pour complets, faute de trace : leur état se vérifie en comptant
les tuiles présentes, sachant que les tuiles hors couverture (la mer) sont
absentes à juste titre.

### Poids du jeu : resume.txt

Le gestionnaire de cartes affiche ce que chaque jeu occupe sur le disque. Le
mesurer demandait de peser les fichiers un par un, et un jeu fin en compte des
centaines de milliers : 12 s pour les seules tuiles d'une clé USB en FAT32,
tout en cache, contre 0,1 s pour le même arbre sur ext4. Le pilote FAT
réanalyse le dossier brut à chaque lecture, là où ext4 tient un index.

La fabrication écrit donc un `resume.txt` dans le dossier de sortie quand elle
aboutit : nombre d'octets, nombre de fichiers. Le gestionnaire le lit au lieu de
peser. L'ouverture de l'écran passe de 14,5 s à 0,8 s sur le jeu complet des
dix cartes, à chiffres identiques.

Le fichier est effacé au démarrage d'une fabrication et réécrit à la fin : sa
présence vaut "ce jeu est complet et rien n'y a bougé depuis". Absent, on
retombe sur la pesée. Le supprimer à la main force donc une nouvelle mesure,
ce qu'il faut faire après avoir ajouté ou retiré des tuiles soi-même.
`fetch_tuiles.py` et `fetch_relief.py` l'écrivent aux mêmes conditions.

## Bâtiments 3D (BD TOPO)

Les bâtiments sont les emprises au sol de la BD TOPO de l'IGN, extrudées à leur
hauteur réelle (murs + toit plat). Les murs sont habillés d'une texture de
façade tuilée (fenêtres, voir `assets/textures/facade.png`, générée par
`tools/facade/generer_facade.py`) répétée en coordonnées réelles (mètres) ;
le toit garde une couleur unie panachée (tuile, ardoise). Ils sont produits à
part par `tools/fetch_buildings.py`, qui interroge le service WFS et écrit
`assets/terrain/<zone>/buildings.bin` :

```bash
tools/.venv/bin/python tools/fetch_buildings.py cote-landes
tools/.venv/bin/python tools/fetch_buildings.py ossau
```

## Ouvrages d'art (bridges.bin)

Le relief vient de RGE ALTI, un modèle numérique de **terrain** : il donne le
sol nu, sans ponts ni viaducs. Un pont n'existe donc que dans l'orthophoto
plaquée dessus, et son tablier suit le creux du lit du fleuve. Mesuré sur le
pont d'Empalot à Toulouse : 8,5 m de creux entre la berge et le milieu de la
Garonne, le tablier plongeait dans l'eau.

La BD TOPO, elle, donne aux tronçons de route et de voie ferrée une géométrie
en trois dimensions, avec l'altitude de la chaussée, et un attribut
`position_par_rapport_au_sol` qui vaut 1 ou plus quand la voie est portée par un
ouvrage. `tools/fetch_bridges.py` en tire un ruban par tronçon et écrit
`assets/terrain/<zone>/bridges.bin` :

```bash
tools/.venv/bin/python tools/fetch_bridges.py toulouse
```

Le moteur extrude chaque ruban en tablier (dessus, dessous, deux flancs) à
l'altitude relevée, dans le même maillage et le même appel de dessin que les
bâtiments. Quelques dizaines de kilooctets par carte, deux secondes de
téléchargement. Une carte sans `bridges.bin` garde ses ponts peints au sol,
comme avant.

Deux écarts assumés, tous deux dus à ce que la BD TOPO ne dit pas :

- **La largeur.** `largeur_de_chaussee` donne le revêtement roulant, pas
  l'ouvrage : ni bande d'arrêt, ni trottoir, ni parapet, et une deux fois deux
  voies arrive en deux tronçons distincts. On ajoute donc 2,5 m de débord de
  chaque côté, plafonnés à 40 % de la chaussée pour ne pas engraisser une
  passerelle. Mesuré au nadir sur l'orthophoto au franchissement de la Garonne
  par l'A620 : 33 m de bitume en travers, dont 13,5 m couverts sans le débord et
  30 m avec.
- **La photo en dessous.** L'orthophoto continue de peindre le pont au niveau du
  sol, dans le lit du fleuve. Le tablier le masque vu d'en haut ; en passant
  dessous, les deux se voient.

La chaussée du tablier, elle, n'est pas grise : elle est **dessinée avec le
shader du terrain** et non avec celui des bâtiments, ce qui lui donne
l'orthophoto et les tuiles de détail drapées par coordonnées monde. Le tablier
montre donc la photo du pont lui-même, marquages et véhicules compris, à la
finesse du sol qui l'entoure. Seuls le dessous et les flancs restent en béton
uni. Aucun shader n'a été écrit pour ça : le drapage se fait en donnant aux
sommets de la chaussée le même UV que le maillage du terrain
(`TerrainMaillage.cpp`), et le dessin passe après le maillage d'ensemble, hors
du pochoir de la fenêtre de relief.

## Masque de forêt (forest.png)

Le semis d'arbres ne jugeait "c'est de la forêt" que sur la couleur du pixel
d'orthophoto : il plantait sur une pelouse sombre, une haie ou une ombre de
versant, et oubliait une forêt en plein soleil. `tools/fetch_forest.py` fabrique
à la place `assets/terrain/<carte>/forest.png` (un pixel tous les 10 m) en
croisant trois couches de l'IGN, chacune pour ce qu'elle sait faire :

```bash
tools/.venv/bin/python tools/fetch_forest.py ossau
```

* **contours de départements** (ADMIN EXPRESS) : jusqu'où va la donnée. En
  France, l'absence de végétation cartographiée VAUT absence d'arbres, sans
  avoir à recenser stades, hippodromes, aérodromes et prairies un par un. Hors de
  France (versant espagnol d'Ossau ou de Cauterets), le semis retombe sur la
  couleur de l'orthophoto, faute de mieux ;
* **zone_de_vegetation** (BD TOPO) : le contour. Levé topographique sans seuil de
  surface, il découpe les autoroutes, les aires de repos, les parkings et les
  stades, et connaît les haies et les bosquets de ville que la BD Forêt ignore ;
* **formation_vegetale** (BD Forêt V2) : l'essence dominante, que la BD TOPO ne
  donne pas (elle dit "Bois", pas "pin maritime"). Elle généralise à 0,5 ha et
  déborde donc sur les routes et les stades : elle ne sert qu'à COLORER
  l'intérieur du contour BD TOPO, jamais à le tracer.

Une dernière passe **gomme** les chaussées goudronnées (BD TOPO
`troncon_de_route`, largeur de chaussée plus six mètres de chaque côté), les
terrains de sport et les pistes d'aérodrome. Sans elle, les haies de bord de
route, larges de trois mètres, gonflent d'un pixel à la rastérisation et
débordent sur la chaussée : des arbres poussaient sur l'A63. Les chemins, les
sentiers et les routes empierrées sont épargnés, eux passent sous le couvert en
vraie forêt.

Chaque essence a sa planche dans `assets/vegetation/trees_atlas.png` : feuillu,
pin, autre conifère, et un tirage entre les deux pour les peuplements mixtes.

Le fichier est facultatif : sans lui, le semis se fait entièrement à la couleur.
Il est lu à sa propre résolution et couvre la même emprise que l'orthophoto ; il
vaut donc aussi pour une carte recadrée, à condition de le regénérer pour elle
(l'emprise vient de son `terrain.txt`).

## Mode zombie

Deux fichiers optionnels par carte activent le mode zombie (combat contre une
horde, roquettes, munitions, vagues à difficulté croissante, score) -- voir
[notice.pdf](notice.pdf) pour les commandes (tir : `Ctrl gauche` au clavier,
clic du stick droit `R3` à la manette) :

* `zombies.txt` : un point de spawn par ligne (`x z`, coordonnées monde), lu
  par le gestionnaire de vagues du combat. À poser sur une carte dédiée
  (ci-dessous), seule façon actuelle de lancer le mode zombie : il n'existe
  pas encore de moyen de démarrer le combat sur une carte de tourisme
  normale.
* `zombie_only.txt` : présence = carte **dédiée** au mode zombie (arène de
  combat, pas d'usage touristique). Le combat démarre directement au
  lancement normal de la carte (`Démarrer`/`Entrée`/bouton `A`). Ces cartes
  sont classées systématiquement en dernier dans la liste du menu (peu
  importe l'ordre alphabétique de leur nom
  de dossier), et leur nuit est figée en permanence (pas de cycle jour/nuit)
  avec la lune visible depuis le cockpit dès le lancement.

La seule carte dédiée fournie est `assets/terrain/dax-arene/` (arène recadrée
depuis `dax`), affichée au menu sous le nom **Happy DeathHour** -- le libellé
vient de la première ligne de son `terrain.txt`, comme pour toute carte (voir
plus haut).

## Cartes

Les cartes fournies avec le simulateur proviennent de l'[IGN](https://www.ign.fr/) (Institut National de l'Information Géographique et Forestière) et sont distribuées sous licence [Licence Ouverte Etalab 2.0](https://www.etalab.gouv.fr/licence-ouverte-open-licence). Ces données sont utilisées pour générer les terrains et les bâtiments 3D du simulateur.

<img src="IGN_logo_2012.png" alt="Logo IGN" width="100" />

Le seuil de hauteur dépend de la zone (clé `height_min` du dictionnaire `ZONES`) :
les bâtiments les plus bas (cabanes, abris) sont écartés pour ne pas alourdir la
scène. Le seuil usuel en ville est de 2 m, relevé à 5 m sur une agglomération très
dense comme Bordeaux, et à 10 m sur Paris ; en montagne (Ossau), il descend à
0 pour garder les cabanes et bergeries, utiles au repérage. Paris intra-muros est
nettement plus dense en bâtiments que Bordeaux à surface égale : son emprise est
donc resserrée à la ville elle-même (sans les communes de la petite couronne), et
son seuil de hauteur relevé à 10 m (le bâti parisien est presque partout un
immeuble haussmannien, largement au-dessus) pour garder un `buildings.bin`
comparable aux autres cartes -- voir le commentaire en tête de
`tools/terrain/zones/paris.py`.

Le moteur charge ce fichier s'il est présent ; sinon, le terrain s'affiche sans
bâtiments. Le bassin d'Arcachon en compte environ 187 000, la côte basco-landaise
environ 156 000, Bordeaux (seuil à 5 m) environ 159 000, Paris (seuil à 10 m)
environ 124 000, la vallée d'Ossau environ 765.
