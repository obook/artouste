# Artouste

![Alouette II en vol dans le simulateur Artouste](docs/artouste-en-vol.png)

Simulateur de pilotage 3D de l'hélicoptère **Aérospatiale Alouette II**
(SE.3130), en C++ moderne et OpenGL. Vol jouable au clavier ou à la manette,
modèle de vol simplifié mais reconnaissable, rendu temps réel sans moteur de
jeu lourd.

## Fonctionnalités

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
