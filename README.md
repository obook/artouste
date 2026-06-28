# Artouste

**Site de présentation : [obook.github.io/artouste](https://obook.github.io/artouste/)**

Artouste est un simulateur de vol personnel consacré au pilotage 3D de l'hélicoptère **Aérospatiale Alouette II** SE.3130.

Son objectif est de restituer fidèlement la séquence de démarrage de la turbine Turboméca puis des rotors, le comportement en vol de cet appareil sans servo-commandes dans une  ambiance sonore caractéristique de sa turbine et de ses trois pales.

Ce n'est ni un jeu ni une reconstitution exhaustive, mais une tentative de retrouver les sensations remarquables de pilotage de cet appareil.

![Alouette II en vol dans le simulateur Artouste](docs/artouste-en-vol.png)

Artouste modélise l'Alouette II SE.3130 avec une précision que FlightGear n'atteint pas sur cet appareil : séquence de démarrage en six états calée sur la turbine Artouste IIC, roue libre simulée, sens de rotation du rotor et compensation anti-couple codés, mode assisté découplé de la physique.

Écrit en C++ moderne et OpenGL, le pilotage est jouable au clavier ou à la manette. Le modèle de vol est simplifié mais reconnaissable, le rendu temps réel sans aucun moteur de jeu.

## Histoire

L'Alouette II n'était pas destinée à devenir une légende. Conçue au milieu des années 1950 par la SNCASE sous la direction de l'ingénieur Charles Marchetti, elle fit son premier vol le 12 mars 1955 à Buc, aux mains du pilote d'essai Jean Boulet. Ce jour-là et pour la première fois, un hélicoptère de série volait grâce à une turbine à gaz.

La turbine Turboméca Artouste IIC changea tout : plus légère, plus fiable, moins gourmande en carburant, elle ouvrit à l'hélicoptère des altitudes que la génération précédente ne pouvait qu'imaginer. Jean Boulet battit en juin 1955 le record mondial d'altitude pour hélicoptère en portant l'Alouette II à 8 209 mètres. La montagne, jusque-là hors de portée, devenait accessible à l'aviation.

La Gendarmerie nationale fut l'une des premières à l'utiliser dès 1957. L'armée de l'air, la Marine puis les armées d'une trentaine de pays suivirent. Plus de 1 300 appareils furent construits entre 1956 et 1975. Certains volent encore aujourd'hui.

Au cinéma, les Alouette apparaissent dans plusieurs films à partir des années 60.

### Films français

Tout l'or du monde (1961), Le Fanfaron (1962), Fantômas (1964), Le Grand Restaurant (1966), Le Tatoué (1968) avec Louis de Funès, La Horse (1969), Peau d'Âne (1970), Le Gendarme en balade (1970), Le Far West (1972) avec Jacques Brel, Le Guignolo (1980) avec Belmondo, La Fille de l'air (1992), Les Rivières pourpres (2000), Les Vacances du Petit Nicolas (2014).

### Films internationaux

You Only Live Twice (James Bond, 1967), The Day of the Jackal (1973), Octopussy (James Bond, 1983), OSS 117 - Atout cœur à Tokyo (1966), Tintin et le Temple du Soleil (1969), Le Cercle rouge (1970), Cassandra Crossing (1976).

Source : [alouettelama.com](https://www.alouettelama.com)

## Fonctionnalités du simulateur

- Modèle de vol Newton-Euler (poussée, gravité, traînée, moments cycliques, anti-couple), effet de sol et effet de translation, intégration à pas fixe.
- Effets liés à l'altitude et au domaine de vol : la portance et la puissance de la turbine décroissent quand on monte, au point d'interdire le stationnaire en haute montagne, conformément à la vocation montagnarde de l'Alouette II. Au-delà de la vitesse à ne pas dépasser (VNE, plus basse en altitude), une traînée d'onde freine l'appareil. Le vol latéral ou arrière prononcé réduit l'autorité au palonnier. Une descente verticale rapide à faible vitesse fait décrocher le rotor (vortex ring state), dont on se dégage en reprenant de la vitesse vers l'avant. Toutes ces difficultés sont désactivées en mode assisté et pendant la démo, où le vol reste facile et prévisible.
- En vue cockpit, une légère vibration de la cabine traduit le passage des trois pales du rotor. L'effet est purement visuel et n'agit pas sur la physique.
- Démarrage et arrêt de la turbine Artouste en deux temps. La turbine monte en   régime, puis le rotor s'accélère, il faut la lancer pour décoller (touche `T`).
- Entrées clavier et manette Xbox (détection automatique de la source).
- Mode assisté (touche `M`) : couche de confort qui compense le lacet, ramène le   cyclique au neutre, lisse les commandes et borne le collectif, sans toucher à la   physique. La bascule est progressive.
- Commandes animées dans la cabine : palonnier, manche cyclique (la main droite   suit) et levier de collectif (la main gauche se pose dessus et le suit).
- Quatre vues (cycle avec `C`) : poursuite, cockpit, orbite et orbite solaire, cette
  dernière plaçant la caméra face au soleil pour mettre en valeur le ciel.
- HUD transparent à trois modes (cycle avec `H`) : panneaux dans les coins,
  instruments ronds verts superposés (Super HUD), ou rien. Le panneau supérieur droit
  affiche l'heure du simulateur (ligne `HRE`), avec un deux-points clignotant.
- Cycle jour/nuit : le soleil suit sa course et colore le ciel au fil des heures, de
  l'aube au coucher orangé puis à la nuit, en orientant l'éclairage de toute la scène.
  La vitesse du temps se règle dans `assets/config.txt` (`sun_time_scale`) : par
  défaut, une journée complète défile en vingt minutes, mais on peut aussi choisir le
  temps réel (heure du PC), un autre rythme, ou figer le temps à midi. La nuit, les deux
  feux de position avant s'allument, rouge à bâbord et vert à tribord.
- Mode démo automatique (touche `V`) : l'appareil joue seul, en boucle, un vol
  panoramique au-dessus du bassin d'Arcachon (décollage, survol de la Dune du Pilat à
  2000 m, passage bas sur la pointe nord du cap Ferret, survol d'Arcachon à 1000 m,
  retour et pose). Un panneau de confirmation s'affiche avant le lancement. Pendant la
  démo, la touche `Échap`, ou une action franche sur le manche, en sort ; la vue
  (`C` ou bouton `Y`), le HUD (`H` ou bouton `B`) et la radio (`K`, `-`/`+`) restent
  actifs sans l'interrompre.
- Son du moteur et du rotor, ciel en dégradé, ombre portée.
- Radio internet optionnelle (touche `K`) : un flux MP3 configurable joué dans le
  cockpit sous les sons moteur, avec un voyant `RADIO` dans le HUD.
- Effets moteur quand la turbine tourne, flash rouge anti-collision sur le toit de
  la cabine et tuyère (distorsion thermique de l'air chaud, halo bleuté à la sortie de la turbine).
- Modèle 3D réel optionnel (voir ci-dessous) ; sinon, hélicoptère procédural.

## Démarrage rapide

> [!TIP]
> **Première fois aux commandes ?**
> Voici les quatre étapes pour effectuer sans stress votre premier vol aux commandes de l'**Alouette II SE.3130**.
>
> 1. **Activez le HUD complet** avec tous les instruments : touche `H` | bouton `B`.
> 2. **Activez le mode assisté** : touche `M` | croix directionnelle haut, un message à l'écran le confirme.
> 3. **Démarrez** la turbine avec la touche `T` | bouton `Start`, la préparation avant de pouvoir décoller prend environ 60 secondes :
>    * La turbine monte seule en régime jusqu'à 33 500 tr/min
>    * Le frein rotor est automatiquement relâché, les pales commencent leur rotation puis le rotor atteint son régime de vol de 360 tr/min. Le HUD indique l'état `"EN RÉGIME"` ou `360`
> 4. **Décollage et vol.** Une fois le rotor en régime :
>    * augmentez doucement le collectif (`W` ou `Z` | gâchette RT) jusqu'à un peu plus de 50% pour décoller de quelques mètres du sol
>    * inclinez l'appareil au cyclique (flèches du clavier | stick gauche) pour avancer
>    * compensez le couple avec le palonnier (`D` - droite / `A` ou `Q` - gauche | stick droit)
>    * réduisez le collectif (`S` | gâchette LT) pour redescendre.

## Commandes

| Action                  | Clavier        | Manette              |
|-------------------------|----------------|----------------------|
| Collectif +/-           | `W`/`Z` / `S`  | RT / LT              |
| Cyclique                | flèches        | stick gauche         |
| Palonniers              | `D` / `A`/`Q`  | stick droit (X)      |
| Turbine (démarrer/couper) | `T`          | bouton `Start`       |
| Vue (poursuite/cockpit/orbite/orbite solaire) | `C` | bouton `Y` (jaune) |
| Livrée (Gendarmerie/origine) | `L`       | bouton `A` (vert)    |
| Mode assisté (confort)  | `M`            | croix directionnelle haut |
| HUD (coins/superposé/aucun) | `H`        | bouton `B`           |
| Pause                   | `P`            | bouton `Back`        |
| Démo : lancer / sortir  | `V` / `Échap`  | sortir : reprendre le manche |
| Radio internet (allumer/couper) | `K`    | -                    |
| Balance radio/hélico    | `-` / `+`      | -                    |
| Reset position          | `R`            | bouton `X`           |
| Quitter                 | `Échap`        | `LB` + `RB`          |

## Téléchargement

Des exécutables prêts à l'emploi pour Linux et Windows sont publiés dans la
section [Releases](https://github.com/obook/artouste/releases) du dépôt. Chaque
archive est autonome : décompressez-la et lancez `artouste` (Linux) ou
`artouste.exe` (Windows), les ressources sont à côté du binaire. Les archives
sont construites automatiquement par GitHub Actions à chaque version
(voir `.github/workflows/release.yml`).

## Compilation (Linux)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/bin/artouste
```

Dépendances récupérées automatiquement (FetchContent) : GLFW, GLM, Dear ImGui,
Assimp, stb, miniaudio, Catch2 ; GLAD est versionné dans `third_party/`.
Prérequis système : pilotes OpenGL, bibliothèques de développement X11, et un
compilateur C++20. libcurl (paquet de développement) est une dépendance système
**optionnelle** : présente, elle active la radio internet ; absente, le simulateur
se compile et tourne normalement sans cette fonctionnalité.

## Compilation (Windows)

Avec Visual Studio 2022 (MSVC) et CMake. Dans une invite de commande Developer :

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
build\Release\artouste.exe
```

Les bibliothèques tierces et le runtime C++ sont liés en statique : l'exécutable
est autonome, sans DLL ni redistribuable Visual C++ à installer.

### Avec VSCode

La configuration partagée est dans `.vscode/`. Lancer une fois la tâche
"CMake : configurer" (menu Terminal > Exécuter la tâche), puis :

- Ctrl+Maj+B : compiler (tâche "CMake : compiler").
- F5 : compiler puis lancer le simulateur sous gdb.
- Tâches "CMake : tester", "Artouste : lancer", "CMake : nettoyer" pour le reste.

L'IntelliSense s'appuie sur `build/compile_commands.json`.

## Packaging

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cd build && cpack
```

Produit une archive `artouste-<version>-<système>` (`.tar.gz` sous Linux,
`.zip` sous Windows) contenant le binaire autonome et toutes les ressources
(shaders, modèle 3D, sons, terrain, textures), prête à distribuer.

## Modèle 3D et sons

Le modèle 3D de l'Alouette II et les sons proviennent du paquet **FlightGear**
de Emmanuel Baranger (helijah), sous licence GPL. Le sous-ensemble utilisé par
le simulateur (modèles `.ac`, textures, quatre boucles sonores rotor et turbine en
intérieur et extérieur, plus le son de démarrage) est inclus dans ce dépôt avec le
fichier `COPYING` d'origine. Source :
<http://helijah.free.fr/flightgear/les-appareils/alouette2/appareil.htm>. S'ils
sont absents, l'application affiche un hélicoptère procédural et reste
silencieuse.

## Hélipad (régénérer l'asset)

L'hélipad de la zone de départ est un modèle `.ac` texturé (`assets/models/helipad/`),
fabriqué avec Blender. Le `.ac` et sa texture sont **versionnés** : compiler et
lancer le simulateur ne demande donc **ni Blender ni greffon**. Les étapes
ci-dessous ne servent qu'à le **régénérer** après modification.

1. Texture (béton, anneau, H rouge), via un environnement Python isolé (hors
   dépôt, voir `.gitignore`) :

```bash
python3 -m venv tools/.venv
tools/.venv/bin/pip install Pillow
tools/.venv/bin/python tools/helipad/make_texture.py assets/models/helipad/helipad.png
```

2. Modèle `.ac`, via Blender et le greffon AC3D d'Emmanuel
   (<https://github.com/NikolaiVChr/Blender-AC3D>, fork de l'original
   <https://github.com/majic79/Blender-AC3D>), installé comme module
   `io_scene_ac3d` :

```bash
blender --background --python tools/helipad/make_helipad.py
```

   Compatibilité Blender < 4.1 : la version actuelle du greffon cible Blender 4.3
   et son importateur appelle `Mesh.set_sharp_from_angle()`, une API qui n'existe
   qu'à partir de Blender 4.1. Sous Blender 4.0, l'import d'un `.ac` plante donc
   tant que ce point n'est pas corrigé. Le contournement consiste à garder cet
   appel dans `io_scene_ac3d/import_ac3d.py` (vers la ligne 585) :

```python
   if hasattr(me, "set_sharp_from_angle"):
       me.set_sharp_from_angle(angle=radians(self.crease))
   elif hasattr(me, "use_auto_smooth"):
       me.use_auto_smooth = True
       me.auto_smooth_angle = radians(self.crease)
```

   Ce correctif ne touche que l'installation locale du greffon, pas le dépôt : les
   `.ac` et leurs textures étant versionnés, compiler et lancer le simulateur ne
   demandent ni Blender ni greffon.

## Rotor de queue (régénérer le skin des pales)

Les pales du rotor de queue sont peintes par un outil Blender qui importe
`blade.ac`, lit les UV et l'envergure, puis colorie les triangles de la pale :
métal nu en livrée d'origine (`tailrotor.png`), jaune à zébrures rouges en livrée
Gendarmerie (`tailrotor-gendarmerie.png`). Les deux textures sont versionnées ;
régénérer ne sert qu'après modification :

```bash
blender --background --python tools/livree/make_tailrotor.py
```

Un contrôle rapide des textures produites :

```bash
tools/.venv/bin/python tools/livree/check_tailrotor.py \
    assets/models/Alouette-II/Models/Externals/TailRotor/tailrotor-gendarmerie.png --zebra
```

## Terrain (choix de la map)

Chaque terrain est rangé dans son propre sous-dossier de `assets/terrain/`, par
exemple `assets/terrain/ossau/` (vallée d'Ossau, montagne),
`assets/terrain/cote-landes/` (côte basco-landaise, de Bayonne à Vieux-Boucau),
`assets/terrain/arcachon/` (bassin d'Arcachon, du Cap Ferret à Marcheprime,
de Biscarrosse à Arès), `assets/terrain/cauterets/` (Cauterets - Gavarnie :
chemin des cascades, Pont d'Espagne, cirque de Gavarnie, montagne) et
`assets/terrain/bordeaux/` (Bordeaux et son agglomération : la Garonne, l'aéroport
de Mérignac, Pessac, Cenon et Lormont).
Un sous-dossier contient `terrain.txt` (calage), `heightmap.png` (relief),
`ortho.jpg` (orthophoto), `landmarks.txt` (lieux remarquables) et, facultatifs,
`helipads.txt` (hélipads à poser, par exemple un hôpital ou un port ; un par
ligne : `lon lat nom`) et `buildings.bin` (bâtiments 3D). L'hélipad de la zone de
départ est toujours présent en plus de ceux de `helipads.txt`.

### Modifier la configuration

Le fichier `assets/config.txt` règle le lancement. C'est un simple fichier texte,
modifiable dans **n'importe quel éditeur**. Chaque ligne est une `clé valeur` ;
une ligne qui commence par `#` est un commentaire (ignoré).

Ce fichier est ta configuration personnelle et n'est donc pas versionné. S'il est
absent au lancement, le simulateur le crée automatiquement en recopiant le modèle
`assets/config.default.txt` (lui, versionné), puis charge cette copie. Tu peux ainsi
modifier `assets/config.txt` à ta guise, ou le supprimer pour repartir des valeurs
par défaut. Les clés disponibles :

- `terrain` : choisit la map chargée au démarrage (voir ci-dessous).
- `turbine_demarree` : `1` pour démarrer avec la **turbine et le rotor déjà au
  régime** (au lieu de la séquence de démarrage d'environ une minute), pratique
  pour décoller tout de suite en test ; `0` (défaut) pour un démarrage normal à
  froid. La variable d'environnement `ARTOUSTE_TURBINE_DEMARREE` a la priorité
  (`ARTOUSTE_TURBINE_DEMARREE=1 ./build/bin/artouste`).
- `demo` : `1` pour lancer le **mode démo automatique** au démarrage (vol joué tout
  seul, en boucle ; le terrain est alors forcé sur `arcachon`) ; `0` (défaut) sinon.
  La touche `V` lance la démo ; `Échap`, ou une action sur le manche, en sort.
  Pendant la démo, la vue (`C` ou bouton `Y`), le HUD (`H` ou bouton `B`) et la radio
  (`K`, `-`/`+`) restent actifs sans l'interrompre. La variable d'environnement
  `ARTOUSTE_DEMO` a la priorité.
- `radio_url` : URL d'un **flux radio internet** (MP3 sur HTTP) joué dans le
  cockpit, sous les sons moteur. Vide par défaut (pas de radio). La radio est
  **coupée au lancement** : la touche `K` l'allume puis la coupe en vol. La
  variable d'environnement `ARTOUSTE_RADIO_URL` a la priorité. La radio est une
  fonctionnalité optionnelle : sans libcurl à la compilation, URL vide ou réseau
  coupé, le simulateur reste silencieux sur ce point, sans erreur. Un voyant
  `RADIO` s'affiche dans le HUD tant que le flux joue, suivi de la part de la radio
  dans le mixage. Les touches `-` et `+` règlent la **balance radio/hélico** (un
  crossfade : monter la radio atténue d'autant le son de l'hélico, et inversement).
- `sun_time_scale` : règle la **vitesse du temps** du cycle jour/nuit, c'est-à-dire
  la rapidité de la course du soleil. La durée réelle d'une journée complète vaut
  `86400 / sun_time_scale` secondes. La valeur `72` (défaut) fait défiler une journée
  entière en vingt minutes ; `144` la réduit à dix minutes ; `1` correspond au temps
  réel, le soleil partant de l'heure locale du PC ; `0` fige le temps à midi (le soleil
  ne bouge plus). Pour toute valeur autre que `1`, le simulateur démarre à midi, afin
  d'ouvrir sur une belle lumière. L'heure courante s'affiche dans le HUD, sur la ligne `HRE` du panneau
  supérieur droit.

Par exemple, pour passer de la vallée d'Ossau à la côte landaise, ouvre
`assets/config.txt` et remplace :

```
terrain ossau
```

par :

```
terrain cote-landes
```

Enregistre, puis relance le simulateur : la nouvelle map est chargée. La valeur
doit être le nom exact d'un sous-dossier de `assets/terrain/` (ici `ossau`,
`cote-landes`, `arcachon`, `cauterets` ou `bordeaux`).

Sans modifier le fichier, la variable d'environnement `ARTOUSTE_TERRAIN` a la
priorité, pratique pour essayer une map ponctuellement :

```bash
ARTOUSTE_TERRAIN=cote-landes ./build/bin/artouste
```

### Régénérer ou ajouter un terrain

Les terrains sont produits hors-ligne par `tools/fetch_terrain.py` (données IGN
Géoplateforme, Licence Ouverte Etalab 2.0). Le script prend le nom de la zone en
argument et écrit dans `assets/terrain/<zone>/` :

```bash
tools/.venv/bin/pip install Pillow numpy scipy   # une fois
tools/.venv/bin/python tools/fetch_terrain.py cote-landes
```

Pour ajouter une zone, copier une entrée du dictionnaire `ZONES` en tête du
script (bornes géographiques, mer ou montagne, point de départ, lieux
remarquables, hélipads). Voir `docs/TERRAIN.md` pour les détails du pipeline.

### Bâtiments 3D (BD TOPO)

Les bâtiments sont les emprises au sol de la BD TOPO de l'IGN, extrudées à leur
hauteur réelle (murs + toit plat). Ils sont produits à part par
`tools/fetch_buildings.py`, qui interroge le service WFS et écrit
`assets/terrain/<zone>/buildings.bin` :

```bash
tools/.venv/bin/python tools/fetch_buildings.py cote-landes
tools/.venv/bin/python tools/fetch_buildings.py ossau
```

Le seuil de hauteur dépend de la zone (clé `height_min` du dictionnaire `ZONES`) :
les bâtiments les plus bas (cabanes, abris) sont écartés pour ne pas alourdir la
scène. Le seuil usuel en ville est de 2 m, relevé à 5 m sur une agglomération très
dense comme Bordeaux ; en montagne (Ossau), il descend à 0 pour garder les cabanes et
bergeries, utiles au repérage.

Le moteur charge ce fichier s'il est présent ; sinon, le terrain s'affiche sans
bâtiments. Le bassin d'Arcachon en compte environ 187 000, la côte basco-landaise
environ 156 000, Bordeaux (seuil à 5 m) environ 159 000, la vallée d'Ossau environ 765.

## Contributions

Le ciel réaliste et la vue d'orbite solaire proviennent d'une contribution de
[CHAT-DISPARU](https://github.com/CHAT-DISPARU), proposée via une pull request. Elle
refond le rendu du ciel, dont le dégradé passe désormais du plein jour aux teintes
orangées du coucher puis à la nuit, avec le disque du soleil et son halo ; elle
introduit aussi un soleil mobile et une nouvelle vue, l'orbite solaire, où la caméra
se place face à l'astre. Le cycle jour/nuit réglable et l'horloge du HUD prolongent
cette base.

## Licence

Ce projet est distribué sous licence **GPL v2** (voir `LICENSE`), comme le
modèle 3D et les sons d'Emmanuel Baranger qu'il inclut.
