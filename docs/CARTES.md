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
Vincennes) et `assets/terrain/dax-arene/` (arène recadrée depuis `dax`, dédiée
au mode zombie -- voir la section dédiée ci-dessous, affichée au menu sous le
nom **Happy DeathHour**).
Un sous-dossier contient `terrain.txt` (calage), `heightmap.png` (relief),
`ortho.jpg` (orthophoto), `landmarks.txt` (lieux remarquables) et, facultatifs,
`helipads.txt` (hélipads à poser, par exemple un hôpital ou un port ; un par
ligne : `lon lat nom`), `hapi.txt` (balise HAPI sur le pad de départ, un par
ligne : `lon lat azimut_deg pente_pct nom` -- voir la section HAPI du README),
`buildings.bin` (bâtiments 3D), `zombies.txt` et `zombie_only.txt` (mode
zombie, voir ci-dessous). L'hélipad de la
zone de départ est toujours présent en plus de ceux de `helipads.txt`.

## Modifier la configuration

Le fichier `assets/config.txt` règle le lancement. C'est un simple fichier texte,
modifiable dans **n'importe quel éditeur**. Chaque ligne est une `clé valeur` ;
une ligne qui commence par `#` est un commentaire (ignoré).

Ce fichier est ta configuration personnelle et n'est donc pas versionné. S'il est
absent au lancement, le simulateur le crée automatiquement en recopiant le modèle
`assets/config.default.txt` (lui, versionné), puis charge cette copie. Tu peux ainsi
modifier `assets/config.txt` à ta guise, ou le supprimer pour repartir des valeurs
par défaut. Les clés disponibles :

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
* `radio_url` : URL d'un **flux radio internet** (MP3 sur HTTP) joué dans le
  cockpit, sous les sons moteur. Vide par défaut (pas de radio). La radio est
  **coupée au lancement** : la touche `K` l'allume puis la coupe en vol. La
  variable d'environnement `ARTOUSTE_RADIO_URL` a la priorité. La radio est une
  fonctionnalité optionnelle : sans libcurl à la compilation, URL vide ou réseau
  coupé, le simulateur reste silencieux sur ce point, sans erreur. Un voyant
  `RADIO` s'affiche dans le HUD tant que le flux joue, suivi de la part de la radio
  dans le mixage. Les touches `-` et `+` règlent la **balance radio/hélico** (un
  crossfade : monter la radio atténue d'autant le son de l'hélico, et inversement).
* `sun_time_scale` : règle la **vitesse du temps** du cycle jour/nuit, c'est-à-dire
  la rapidité de la course du soleil. La durée réelle d'une journée complète vaut
  `86400 / sun_time_scale` secondes. La valeur `72` (défaut) fait défiler une journée
  entière en vingt minutes ; `144` la réduit à dix minutes ; `1` correspond au temps
  réel, le soleil partant de l'heure locale du PC ; `0` fige le temps à midi (le soleil
  ne bouge plus). Pour toute valeur autre que `1`, le simulateur démarre à midi, afin
  d'ouvrir sur une belle lumière. L'heure courante s'affiche dans le HUD, sur la ligne `HRE`
  du panneau supérieur droit.
* `tree_max` : **budget de végétation**, soit le nombre maximum d'arbres soumis à la
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

Un jeu de tuiles n'est pas versionné (donnée dérivée, régénérable, sans commune
mesure avec le dépôt). Pour l'empaqueter en vue d'une release :

```bash
cmake -S . -B build -DARTOUSTE_TUILES_DIR=/media/disque/tuiles
cmake --build build --target tuiles     # écrit build/tuiles-<carte>.zip
```

### Où le jeu cherche les tuiles

Dans cet ordre :

1. `$ARTOUSTE_TUILES/<carte>/`, si la variable d'environnement est définie ;
2. `assets/terrain/<carte>/tuiles/`.

Le premier chemin existe pour les gros jeux qu'on préfère garder sur un autre
disque. Tout est facultatif : carte sans tuiles, dossier absent, tuile manquante
ou abîmée, pilote sans BC7, `tuiles_fenetre_px 0` dans la configuration -- dans
tous ces cas le terrain s'affiche avec sa seule orthophoto d'ensemble.

Une tuile hors couverture BD ORTHO (blanc pur, au-delà de la frontière espagnole
sur les cartes de montagne) n'est volontairement PAS écrite : le moteur y
retombe sur l'orthophoto d'ensemble, qui a été recousue à la préparation de la
carte, plutôt que de plaquer un carré blanc sur le paysage.

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

## Mode zombie

Deux fichiers optionnels par carte activent le mode zombie (combat contre une
horde, roquettes, munitions, vagues à difficulté croissante, score) -- voir
[notice.pdf](notice.pdf) pour les commandes (tir : `Ctrl gauche` au clavier,
clic du stick droit `R3` à la manette) :

* `zombies.txt` : présence = carte **compatible** avec le mode zombie. Un
  point de spawn par ligne (`x z`, coordonnées monde). Sur une carte compatible
  mais pas dédiée (ci-dessous), le mode reste optionnel : bouton `Mode Zombie`
  du menu de démarrage, ou touche `Z` / bouton `LB` (Xbox) / `L1`
  (PlayStation) une fois la carte sélectionnée.
* `zombie_only.txt` : présence = carte **dédiée** au mode zombie (arène de
  combat, pas d'usage touristique). Le combat démarre directement au
  lancement normal de la carte (`Démarrer`/`Entrée`/bouton `A`), sans passer
  par le bouton `Mode Zombie`. Ces cartes sont classées systématiquement en
  dernier dans la liste du menu (peu importe l'ordre alphabétique de leur nom
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
