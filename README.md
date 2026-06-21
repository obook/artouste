# Artouste

Artouste est un simulateur de vol personnel consacré au pilotage 3D de l'hélicoptère **Aérospatiale Alouette II** SE.3130.

Son objectif est de restituer fidèlement la séquence de démarrage de la turbine Turboméca puis des rotors, le comportement en vol de cet appareil sans servo-commandes dans une  ambiance sonore caractéristique de sa turbine et de ses trois pales.

Ce n'est ni un jeu ni une reconstitution exhaustive, mais une tentative de retrouver les sensations remarquables de pilotage de cet appareil.

![Alouette II en vol dans le simulateur Artouste](docs/artouste-en-vol.png)

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
- Démarrage et arrêt de la turbine Artouste en deux temps. La turbine monte en   régime, puis le rotor s'accélère, il faut la lancer pour décoller (touche `T`).
- Entrées clavier et manette Xbox (détection automatique de la source).
- Mode assisté (touche `M`) : couche de confort qui compense le lacet, ramène le   cyclique au neutre, lisse les commandes et borne le collectif, sans toucher à la   physique. La bascule est progressive.
- Commandes animées dans la cabine : palonnier, manche cyclique (la main droite   suit) et levier de collectif (la main gauche se pose dessus et le suit).
- Trois vues (cycle avec `C`) : poursuite, cockpit, orbite.
- HUD transparent à trois modes (cycle avec `H`) : panneaux dans les coins,
  instruments ronds verts superposés (Super HUD), ou rien.
- Son du moteur et et rotor, ciel en dégradé, ombre portée.
- Effets moteur quand la turbine tourne, flash rouge anti-collision sur le toit de
  la cabine et tuyère (distorsion thermique de l'air chaud, halo bleuté à la sortie de la turbine).
- Modèle 3D réel optionnel (voir ci-dessous) ; sinon, hélicoptère procédural.

## Démarrage rapide

> [!TIP]
> **Première fois aux commandes ?**
> Voici les quatre étapes pour effectuer votre premier vol aux commandes de l'**Alouette II SE.3130**.
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
| Vue (poursuite/cockpit/orbite) | `C`     | bouton `Y` (jaune)   |
| Livrée (Gendarmerie/origine) | `L`       | bouton `A` (vert)    |
| Mode assisté (confort)  | `M`            | croix directionnelle haut |
| HUD (coins/superposé/aucun) | `H`        | bouton `B`           |
| Pause                   | `P`            | bouton `Back`        |
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
compilateur C++20.

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
le simulateur (modèles `.ac`, textures, deux boucles sonores) est inclus dans
ce dépôt avec le fichier `COPYING` d'origine. Source :
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

## Licence

Ce projet est distribué sous licence **GPL v2** (voir `LICENSE`), comme le
modèle 3D et les sons d'Emmanuel Baranger qu'il inclut.
