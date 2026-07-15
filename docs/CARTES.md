# Cartes : choix, configuration, régénération

Chaque terrain est rangé dans son propre sous-dossier de `assets/terrain/`, par
exemple `assets/terrain/ossau/` (vallée d'Ossau, montagne),
`assets/terrain/cote-landes/` (côte basco-landaise, de Bayonne à Vieux-Boucau),
`assets/terrain/arcachon/` (bassin d'Arcachon, du Cap Ferret à Marcheprime,
de Biscarrosse à Arès), `assets/terrain/cauterets/` (Cauterets - Gavarnie :
chemin des cascades, Pont d'Espagne, cirque de Gavarnie, montagne) et
`assets/terrain/bordeaux/` (Bordeaux et son agglomération : la Garonne, l'aéroport
de Mérignac, Pessac, Cenon et Lormont), `assets/terrain/dax/` (Dax : l'Adour,
l'aérodrome de Seyresse et le musée de l'ALAT, les Thermes) et
`assets/terrain/bigorre/` (Pic du Midi de Bigorre : l'observatoire, le col du
Tourmalet, la station de La Mongie).
Un sous-dossier contient `terrain.txt` (calage), `heightmap.png` (relief),
`ortho.jpg` (orthophoto), `landmarks.txt` (lieux remarquables) et, facultatifs,
`helipads.txt` (hélipads à poser, par exemple un hôpital ou un port ; un par
ligne : `lon lat nom`), `hapi.txt` (balise HAPI sur le pad de départ, un par
ligne : `lon lat azimut_deg pente_pct nom` -- voir la section HAPI du README)
et `buildings.bin` (bâtiments 3D). L'hélipad de la
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
`cote-landes`, `arcachon`, `cauterets`, `bordeaux`, `dax` ou `bigorre`).

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

## Bâtiments 3D (BD TOPO)

Les bâtiments sont les emprises au sol de la BD TOPO de l'IGN, extrudées à leur
hauteur réelle (murs + toit plat). Ils sont produits à part par
`tools/fetch_buildings.py`, qui interroge le service WFS et écrit
`assets/terrain/<zone>/buildings.bin` :

```bash
tools/.venv/bin/python tools/fetch_buildings.py cote-landes
tools/.venv/bin/python tools/fetch_buildings.py ossau
```

## Cartes

Les cartes fournies avec le simulateur proviennent de l'[IGN](https://www.ign.fr/) (Institut National de l'Information Géographique et Forestière) et sont distribuées sous licence [Licence Ouverte Etalab 2.0](https://www.etalab.gouv.fr/licence-ouverte-open-licence). Ces données sont utilisées pour générer les terrains et les bâtiments 3D du simulateur.

<img src="IGN_logo_2012.png" alt="Logo IGN" width="100" />

Le seuil de hauteur dépend de la zone (clé `height_min` du dictionnaire `ZONES`) :
les bâtiments les plus bas (cabanes, abris) sont écartés pour ne pas alourdir la
scène. Le seuil usuel en ville est de 2 m, relevé à 5 m sur une agglomération très
dense comme Bordeaux ; en montagne (Ossau), il descend à 0 pour garder les cabanes et bergeries, utiles au repérage.

Le moteur charge ce fichier s'il est présent ; sinon, le terrain s'affiche sans
bâtiments. Le bassin d'Arcachon en compte environ 187 000, la côte basco-landaise
environ 156 000, Bordeaux (seuil à 5 m) environ 159 000, la vallée d'Ossau environ 765.
