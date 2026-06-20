# Artouste

Artouste est un simulateur de vol personnel consacré au pilotage 3D de l'hélicoptère **Aérospatiale Alouette II** (SE.3130). Il cherche à restituer fidèlement la séquence de démarrage de la turbine, les sensations de vol sans servo-commandes et le son caractéristique de sa turbine et de ses trois pales.

Ce n'est ni un jeu ni une reconstitution exhaustive, mais une tentative de retrouver les sensations remarquables de pilotage de cet appareil.

![Alouette II en vol dans le simulateur Artouste](docs/artouste-en-vol.png)

Écrit en C++ moderne et OpenGL, le pilotage est jouable au clavier ou à la manette. Le modèle de vol est simplifié mais reconnaissable, le rendu temps réel sans aucun moteur de jeu.

## Histoire

L'Alouette II n'était pas destinée à devenir une légende. Conçue au milieu des années 1950 par la SNCASE sous la direction de l'ingénieur Charles Marchetti, elle fit son premier vol le 12 mars 1955 à Buc, aux mains du pilote d'essai Jean Boulet. Ce jour-là et pour la première fois, un hélicoptère de série volait grâce à une turbine à gaz.

La turbine Turboméca Artouste IIC changea tout : plus légère, plus fiable, moins gourmande en carburant, elle ouvrit à l'hélicoptère des altitudes que la génération précédente ne pouvait qu'imaginer. Jean Boulet battit en juin 1955 le record mondial d'altitude pour hélicoptère en portant l'Alouette II à 8 209 mètres. La montagne, jusque-là hors de portée, devenait accessible à l'aviation.

La Gendarmerie nationale fut l'une des premières à l'utiliser dès 1957. L'armée de l'air, la Marine puis les armées d'une trentaine de pays suivirent. Plus de 1 300 appareils furent construits entre 1956 et 1975. Certains volent encore aujourd'hui.

Au cinéma, les Alouette apparaissent dans plusieurs films à partir des années 60.

### Films français

Tout l'or du monde (1961), Le Fanfaron (1962), Fantômas (1964), Le Grand Restaurant (1966), Le Tatoué (1968) avec Louis de Funès convainquant le pilote de l'amener à Paris, La Horse (1969), Peau d'Âne (1970), Le Gendarme en balade (1970), Le Far West (1972) avec Jacques Brel suspendu sous l'Alouette, Le Guignolo (1980) avec Belmondo survolant Venise accroché en dessous, La Fille de l'air (1992), Les Rivières pourpres (2000), Les Vacances du Petit Nicolas (2014).

### Films internationaux

You Only Live Twice (James Bond, 1967), The Day of the Jackal (1973), Octopussy (James Bond, 1983), OSS 117 - Atout cœur à Tokyo (1966), Tintin et le Temple du Soleil (1969), Le Cercle rouge (1970), Cassandra Crossing (1976).

Source : [alouettelama.com](https://www.alouettelama.com)

## Fonctionnalités du simulateur

- Modèle de vol Newton-Euler (poussée, gravité, traînée, moments cycliques,
  anti-couple), effet de sol et effet de translation, intégration à pas fixe.
- Démarrage et arrêt de la turbine Artouste en deux temps (la turbine monte en
  régime, puis le rotor s'accélère) : il faut la lancer pour décoller.
- Entrées clavier et manette Xbox (détection automatique de la source).
- Trois vues (cycle avec `C`) : poursuite, cockpit, orbite.
- HUD transparent à trois modes (cycle avec `H`) : panneaux dans les coins,
  instruments ronds verts superposés (Super HUD), ou rien.
- Son moteur et rotor, ciel en dégradé, ombre portée.
- Modèle 3D réel optionnel (voir ci-dessous) ; sinon, hélicoptère procédural.

## Commandes

| Action                  | Clavier        | Manette              |
|-------------------------|----------------|----------------------|
| Collectif +/-           | `W`/`Z` / `S`  | RT / LT              |
| Cyclique                | flèches        | stick gauche         |
| Palonniers              | `D` / `A`/`Q`  | stick droit (X)      |
| Turbine (démarrer/couper) | `T`          | bouton `Start`       |
| Vue (poursuite/cockpit/orbite) | `C`     | bouton `Y` (jaune)   |
| Livrée (Gendarmerie/origine) | `L`       | bouton `A` (vert)    |
| HUD (coins/superposé/aucun) | `H`        | bouton `B`           |
| Pause                   | `P`            | bouton `Back`        |
| Reset position          | `R`            | bouton `X`           |
| Quitter                 | `Échap`        | `LB` + `RB`          |

## Compilation (Linux)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/bin/artouste
```

Dépendances récupérées automatiquement (FetchContent) : GLFW, GLAD, GLM,
Dear ImGui, Assimp, stb, miniaudio, Catch2. Prérequis système : pilotes
OpenGL, bibliothèques X11/Wayland, et un compilateur C++20.

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

Produit une archive `artouste-<version>-<système>.tar.gz` contenant le binaire
et les shaders.

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
