# Artouste

**Site de présentation : [obook.github.io/artouste](https://obook.github.io/artouste/)**

Artouste est un simulateur de vol personnel consacré au pilotage 3D de l'hélicoptère **Aérospatiale Alouette II** SE.3130.

Son objectif est de restituer fidèlement la séquence de démarrage de la turbine Turboméca puis des rotors, le comportement en vol de cet appareil sans servo-commandes dans une ambiance sonore caractéristique de sa turbine et de ses trois pales.

Ce n'est ni un jeu ni une reconstitution exhaustive, mais une tentative de retrouver les sensations remarquables de pilotage de cet appareil.

![Alouette II en vol dans le simulateur Artouste](docs/artouste-en-vol.png)

Artouste modélise l'Alouette II SE.3130 avec une précision que FlightGear n'atteint pas : séquence de démarrage en six états calée sur la turbine Artouste IIC, roue libre simulée, sens de rotation du rotor et compensation anti-couple codés, mode assisté découplé de la physique.

Écrit en C++ moderne et OpenGL, le modèle de vol est simplifié mais reconnaissable, le rendu temps réel sans aucun moteur de jeu.

> [!IMPORTANT]
> Un **PC** et **une manette de jeu** suffisent.
> Aucune installation n'est nécessaire : on extrait l'archive, on lance le programme, on décolle !

## Histoire

L'Alouette II n'était pas destinée à devenir une légende.

Conçue par la Société nationale des constructions aéronautiques du Sud-Est (SNCASE) au
milieu des années 1950, elle fut le premier hélicoptère de série au monde à voler
grâce à une turbine à gaz, ouvrant à l'appareil des altitudes jusque-là hors de
portée. Elle a aussi marqué le cinéma, de Fantômas à James Bond. Voir
[docs/HISTOIRE.md](docs/HISTOIRE.md) pour le récit complet.

<div align="center"><img src="docs/photo/SE3130-ISNCASE-DAX.jpg" alt="SE 3130 - Musée de l'ALAT et de l'hélicoptère - Dax (40)" width="480" />

_Alouette II SNCASE SE.3130 - Musée de l'ALAT et de l'hélicoptère - Dax (40)_</div>

## Démarrage rapide

> [!NOTE]
> **Au lancement**, le simulateur s'ouvre sur un menu de choix : la
> carte à charger et le démarrage éventuel de la turbine. En vol, `Échap` (ou `LB` + `RB` à la manette) ramène à ce menu ; dans le menu, `Échap` quitte le simulateur.
> 
> La notice des commandes est disponible au téléchargement : [notice.pdf](docs/notice.pdf)

> [!TIP]
> **Première fois aux commandes ?**
> Voici les trois étapes pour effectuer sans stress votre premier vol à bord de l'Alouette.
> 1. **Activez le mode assisté** : touche `M` | `LB`, un message à l'écran le confirme.
> 2. **Démarrez la turbine** avec la touche `T` | bouton `Start`, la préparation avant de pouvoir décoller prend environ 60 secondes :
>    * La turbine monte seule en régime jusqu'à 33 500 tr/min
>    * Le frein rotor est automatiquement relâché, les pales commencent leur rotation puis le rotor atteint son régime de vol
> 3. **Décollage et vol.** Une fois le rotor en régime :
>    * augmentez doucement le collectif (touche `Z` | gâchette `RT`) jusqu'à un peu plus de 50 % pour décoller de quelques mètres du sol
>    * inclinez l'appareil au cyclique vers l'avant (flèches | stick gauche) et avancez
>    * compensez le couple avec le palonnier (touche `D` à droite, `A` ou `Q` à gauche | stick droit)
>    * réduisez le collectif (touche `S` | gâchette `LT`) pour redescendre.

> [!NOTE]
> **Envie d'une démo ?**
> À partir du menu, appuyer sur la touche `D` : l'appareil effectue seul,
> en boucle, un vol panoramique au-dessus du bassin d'Arcachon. La touche `Échap` ou une action franche sur le manche en sort.

## Fonctionnalités du simulateur

* Modèle de vol Newton-Euler (poussée, gravité, traînée, moments cycliques, anti-couple), effet de sol et effet de translation, intégration à pas fixe.
* Effets liés à l'altitude et au domaine de vol : la portance et la puissance de la turbine décroissent quand on monte, au point d'interdire le stationnaire en haute montagne (vers 3 300 m), conformément à la vocation montagnarde de l'Alouette II. Au-delà de la vitesse à ne pas dépasser (VNE, plus basse en altitude), une traînée d'onde freine l'appareil. Le vol latéral ou arrière prononcé réduit l'autorité au palonnier. Une descente verticale rapide à faible vitesse fait décrocher le rotor (vortex ring state), dont on se dégage en reprenant de la vitesse vers l'avant. Toutes ces difficultés sont désactivées en mode assisté, pendant la démo et pendant l'atterrissage automatique, où le vol reste facile et prévisible.
* En vue cockpit, une légère vibration de la cabine traduit le passage des trois pales du rotor. L'effet est purement visuel et n'agit pas sur la physique.
* Démarrage et arrêt de la turbine Artouste en deux temps. La turbine monte en régime, puis le rotor s'accélère, il faut la lancer pour décoller (touche `T`).
* Entrées manette (recommandée) et clavier, avec détection automatique de la source. Manette Xbox, ou manette PlayStation 4 (DualShock 4) / PlayStation 5 (DualSense) en USB ou Bluetooth. La base communautaire de correspondances SDL (`assets/gamecontrollerdb.txt`) est chargée au lancement : elle étend la reconnaissance à un large éventail de manettes, dont les modèles Xbox récents en Bluetooth que la base intégrée de GLFW ne couvrait pas.
* Mode assisté (touche `M`) : couche de confort qui compense le lacet, ramène le cyclique au neutre, lisse les commandes et borne le collectif, sans toucher à la physique. La bascule est progressive.
* Atterrissage automatique (touche `J` / `RB`) : engage le pilote automatique vers l'hélipad le plus proche (dans un rayon de 999 m, sinon la touche n'a aucun effet), qui rejoint la pente d'approche du HAPI (6 %), se pose en douceur et rend la main une fois le collectif ramené au sol. Une action franche sur le manche, le palonnier ou le collectif désengage l'atterrissage automatique et rend la main tout de suite.
* Commandes animées dans la cabine : palonnier, manche cyclique (la main droite suit) et levier de collectif (la main gauche se pose dessus et le suit).
* Quatre vues (cycle avec `C`) : poursuite, cockpit, orbite et orbite solaire, cette
  dernière plaçant la caméra face au soleil pour mettre en valeur le ciel.
* HUD transparent à trois modes (cycle avec `H`) : panneaux dans les coins,
  instruments ronds verts superposés (Super HUD) ou rien. Le panneau supérieur droit
  affiche l'heure du simulateur (ligne `HRE`), avec un deux-points clignotant. En mode
  coins, le coin bas-droit indique aussi le nombre d'images par seconde (FPS).
  Cinq paramètres sont surveillés par une alarme à trois états (vert = normal,
  jaune = surveiller, rouge = limite franchie) : régime rotor (autour de la bande
  nominale, inhibée tant que le rotor n'est pas en régime), régime turbine (verte
  au régime, elle sert d'indicateur "turbine prête" au démarrage), vitesse (sur la
  VNE réelle du moment, qui décroît avec l'altitude), température tuyère et
  carburant (jaune sous 60 L, rouge sous 15 L). En Super HUD, l'alarme est une LED
  en haut à droite du cadran ; en HUD coins, la ligne concernée passe au jaune ou
  au rouge.
* Balise HAPI (Helicopter Approach Path Indicator) sur le pad de départ de chaque
  carte : un repère au sol qui indique la pente d'approche par sa couleur, selon
  les seuils du guide DGAC/STAC : vert clignotant si trop haut, vert fixe sur la
  pente, rouge fixe légèrement trop bas, rouge clignotant si trop bas. Le point de
  l'étiquette HUD (coin ou minimap) reprend cette couleur, sur l'étiquette la plus
  proche de la balise et sur elle seule, pour qu'une balise n'en éclaire jamais
  deux ; hors des seuils, le point s'éteint plutôt que de passer à une couleur
  intermédiaire. Données par carte dans un fichier optionnel
  `hapi.txt` (`lon lat azimut_deg pente_pct nom`), voir [docs/CARTES.md](docs/CARTES.md).
* Interface à l'échelle de la fenêtre : le HUD (rubans, cadrans, réticule, minimap,
  étiquettes) et le menu de démarrage suivent la taille réelle de l'affichage
  (référence 1280x720, facteur borné de 0,75 à 3,5), et la police est reconstruite
  nette à chaque changement de taille. L'affichage reste lisible et proportionné
  du petit fenêtré au plein écran 4K, y compris en fenêtre étroite.
* Cycle jour/nuit : le soleil suit sa course et colore le ciel au fil des heures, de
  l'aube au coucher orangé puis à la nuit, en orientant l'éclairage de toute la scène.
  La vitesse du temps se règle dans `assets/config.txt` (`soleil_vitesse`) : par
  défaut, une journée complète défile en vingt minutes, mais on peut aussi choisir le
  temps réel (heure du PC), un autre rythme, ou figer le temps à 8h du matin. La nuit, les deux
  feux de position avant s'allument, rouge à bâbord et vert à tribord.
* Mode démo automatique (bouton `Démo` du menu de démarrage) : l'appareil joue seul,
  en boucle, un vol panoramique au-dessus du bassin d'Arcachon (démarrage accéléré de
  la turbine puis embrayage du rotor à vitesse réelle, décollage, survol de la Dune du
  Pilat à 2000 m, passage bas sur la pointe nord du cap Ferret, survol d'Arcachon à
  1000 m, retour et pose). Pendant la démo, la touche `Échap`, ou une action franche
  sur le manche, en sort et ramène au menu de démarrage ; la vue (`C` ou bouton `Y`),
  le HUD (`H` ou bouton `B`), le plein écran (`F`) et la radio (`K`, `-`/`+`) restent
  actifs sans l'interrompre.
* Son du moteur et du rotor, ciel en dégradé, ombre portée.
* Radio internet optionnelle (touche `K`) : un flux MP3 configurable joué dans le
  cockpit sous les sons moteur, avec un voyant `RADIO` dans le HUD.
* Effets moteur quand la turbine tourne, flash rouge anti-collision sur le toit de
  la cabine et tuyère (distorsion thermique de l'air chaud, halo bleuté à la sortie de la turbine).
* Souffle du rotor au ras du sol : sous une quinzaine de mètres, l'appareil soulève
  un nuage de poussière qui s'écarte en anneau puis remonte autour de lui, d'autant
  plus dense qu'il est près du sol, que le rotor tourne vite et que le pas est fort :
  un nuage au contact, un voile à cinq mètres, une trace à dix. Sa
  couleur est prise sur la photo aérienne du terrain, donc ocre sur une piste,
  grise sur un éboulis, et remplacée par des embruns au-dessus de l'eau. La clé
  `souffle` de la configuration l'éteint.
* Modèle 3D réel optionnel (voir ci-dessous) ; sinon, hélicoptère procédural.

## Commandes

La liste complète des commandes est dans la notice :
**[notice.pdf](docs/notice.pdf)**. Elle couvre le vol, les vues, la livrée, le
mode assisté, l'atterrissage automatique, le HUD, la radio, la démo, le
gestionnaire de cartes et le mode zombie, en deux versions schéma à l'appui,
manette Xbox puis manette PlayStation.

Elle n'est pas recopiée ici : deux listes à tenir finissent par diverger, et
c'était déjà le cas (le tableau qui occupait cette place ignorait le mode
zombie, le tir et le gestionnaire de cartes).

Une manette PlayStation 4 (DualShock 4) ou PlayStation 5 (DualSense) branchée en USB ou en Bluetooth fonctionne au même titre qu'une manette Xbox : le mappage GLFW/SDL fait correspondre `A`/`B`/`X`/`Y` à Croix/Rond/Carré/Triangle, `Start` à `Options` et `Back` à `Share`/`Create`.

### Manettes essayées

Le simulateur en reconnaît bien davantage (voir plus bas), mais ces modèles-là ont été essayés manette en main :

| Manette | Connexion | Remarque |
|---|---|---|
| Xbox 360, One, Series X/S | USB, Bluetooth | rien à faire |
| PlayStation 4 (DualShock 4) | USB, Bluetooth | rien à faire |
| PlayStation 5 (DualSense) | USB, Bluetooth | rien à faire |
| Gamepad Freebox, et les autres manettes DragonRise PC TWIN SHOCK | USB | mode analogique obligatoire, diode rouge allumée |

Cette reconnaissance étendue s'appuie sur la base communautaire SDL embarquée (`assets/gamecontrollerdb.txt`), chargée au démarrage. Elle couvre plus de 2 200 manettes (2 242 à ce jour) sur Linux, Windows et macOS, dont les modèles Xbox sans fil récents (Series X/S) en Bluetooth, absents de la base intégrée de GLFW. Pour rester à jour au fil des nouveaux modèles, il suffit de remplacer ce fichier par la dernière version publiée sur le dépôt [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB).

Un second fichier, `assets/gamecontrollerdb-extra.txt`, est chargé juste après et prime sur le premier. Il corrige ou complète la base communautaire sans la modifier, pour que celle-ci reste remplaçable telle quelle. Il contient aujourd'hui la manette DragonRise PC TWIN SHOCK (USB `0079:0006`), vendue aussi sous les noms "Microntek USB Joystick" ou "USB Gamepad", et distribuée par Free sous le nom de Gamepad Freebox : les correspondances existantes la décrivaient avec cinq axes alors qu'elle n'en expose que quatre, ce qui suffisait à la faire rejeter en bloc et à la rendre inutilisable.

Sur ce modèle, le mode analogique est obligatoire. Diode éteinte, le stick droit n'envoie pas d'axes mais recopie les quatre boutons de face : le palonnier reste mort pendant que le stick coupe la turbine, change de vue et remet l'appareil au point de départ. Un appui sur le bouton ANALOG au centre de la manette allume la diode rouge et rétablit les deux sticks.

Si une manette n'est pas reconnue, l'outil `gamepad_probe` livré avec les sources affiche son GUID, le nombre d'axes, de boutons et de chapeaux, ainsi que leur état brut en direct. Il donne tout ce qu'il faut pour écrire la ligne correspondante ; le format est rappelé dans l'entête de `gamecontrollerdb-extra.txt`.

```sh
./build/bin/gamepad_probe
```

## Téléchargement

Des exécutables prêts à l'emploi pour Linux et Windows sont publiés dans la
section [Releases](https://github.com/obook/artouste/releases) du dépôt. Chaque
archive est autonome : décompressez-la et lancez `artouste` (Linux) ou
`artouste.exe` (Windows), les ressources sont à côté du binaire. Les archives
sont construites automatiquement par GitHub Actions à chaque version
(voir `.github/workflows/release.yml`).

<details>
<summary>Publier une version (mémo)</summary>

1. Écrire les notes dans `docs/RELEASE_NOTES.md`, bumper `VERSION` dans
   `CMakeLists.txt`, commiter et pousser.
2. Publier :
   ```bash
   ./scripts/release.sh v0.30.0
   ```

Le script pose le tag, le pousse (la compilation des deux plateformes démarre
et prend une quinzaine de minutes) et crée aussitôt la release, avec le seul
texte de cette version. La CI se contente ensuite d'y attacher les deux
archives.

Créer la release depuis le poste, et non depuis la CI, n'est pas décoratif :
l'auteur d'une release est figé à sa création et ne se change plus ensuite.
Créée par l'action, elle porte le compte du robot, qui apparaît alors parmi les
contributeurs du projet. Tant que c'était une étape séparée, elle a été oubliée
trois fois ; le workflow refuse désormais de créer une release lui-même.
</details>

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

* Ctrl+Maj+B : compiler (tâche "CMake : compiler").
* F5 : compiler puis lancer le simulateur sous gdb.
* Tâches "CMake : tester", "Artouste : lancer", "CMake : nettoyer" pour le reste.

L'IntelliSense s'appuie sur `build/compile_commands.json`.

## Packaging

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cd build && cpack
```

Produit une archive `artouste-<version>-<système>` (`.tar.gz` sous Linux,
`.zip` sous Windows) contenant le binaire autonome, les ressources (shaders,
modèle 3D, sons, textures) et **les neuf cartes**, prête à distribuer. Comptez
environ 140 Mo.

Une carte se conditionne aussi à part, pour la passer à quelqu'un sans lui
envoyer l'archive entière :

```bash
cmake --build build --target cartes
```

Produit un `build/carte-<nom>.zip` par carte. La liste est établie à la
configuration : après avoir ajouté une carte dans `assets/terrain/`, relancez
`cmake` avant de reconstruire cette cible.

## Modèle 3D, sons et textures

Le modèle 3D et les sons de l'Alouette II proviennent du paquet FlightGear
d'Emmanuel Baranger (licence GPL) ; à défaut, le simulateur affiche un hélicoptère
procédural et reste silencieux. Les textures des livrées, du rotor de queue et de
l'hélipad sont versionnées et n'ont besoin d'être régénérées qu'après
modification. Détails et commandes de régénération : [docs/ASSETS.md](docs/ASSETS.md).

## Cartes (terrains)

**Les neuf cartes sont livrées avec le jeu**, dans l'archive : Ossau, côte
landaise, Happy DeathHour (l'arène du mode zombie), Dax, Bordeaux, Paris, bassin
d'Arcachon, Cauterets-Gavarnie et Pic du Midi de Bigorre. À choisir dans
`assets/config.txt` (clé `terrain`), au menu de démarrage, ou via la variable
d'environnement `ARTOUSTE_TERRAIN`.

Elles sont en basse résolution, ce qui suffit en vol haut : leur photo aérienne
d'ensemble tient en une quinzaine de mégaoctets par carte. La haute résolution,
elle, se télécharge depuis le jeu (voir le gestionnaire de cartes plus bas).

Une carte ajoutée à la main dans `assets/terrain/` apparaît au menu au lancement
suivant, sans rien configurer : le jeu recense ce dossier.

### Reprendre un vol à un endroit précis

Le simulateur accepte des options de lancement qui sautent le menu et posent
l'appareil là où on en a besoin, au lieu de le faire convoyer depuis le pad.
Pratique pour aller regarder un monument, un sommet ou un défaut de terrain sans
refaire le trajet à chaque essai.

```sh
artouste --carte paris --monument "Pantheon" --alt 300
artouste --carte paris --lieu "Tour Eiffel" --alt 200 --cap 270
artouste --carte paris --lon 2.346073 --lat 48.846167 --alt 300
```

`--monument` cherche le nom dans le `monuments.txt` de la carte, `--lieu` dans
son `landmarks.txt`. La recherche ignore la casse, les accents et la
ponctuation, et un fragment suffit : `pantheon` trouve `Panthéon`. Nom
introuvable, le vol commence au pad, avec un message.

`--alt` est une hauteur **au-dessus du sol**, pas une altitude absolue ; 0 pose
l'appareil sur le relief. Dès qu'un point d'apparition est demandé, la turbine et
le rotor sont mis au régime : naître en vol moteur arrêté, c'est tomber.

`artouste --aide` donne la liste complète. Les variables d'environnement
`ARTOUSTE_*` restent lues et gardent la priorité sur les options équivalentes.

Les terrains et bâtiments 3D sont générés hors-ligne depuis les données de
l'[IGN](https://www.ign.fr/) (Licence Ouverte Etalab 2.0). Détails, configuration
complète et régénération : [docs/CARTES.md](docs/CARTES.md) ; étude du pipeline de
terrain : [docs/TERRAIN.md](docs/TERRAIN.md).

<img src="docs/IGN_logo_2012.png" alt="IGN" width="64" />

### Première ouverture d'une carte

L'orthophoto est livrée en JPEG, que la carte graphique ne sait pas lire
directement. Au premier chargement d'une carte, le jeu la convertit en texture
compressée et range le résultat dans le cache de votre compte
(`~/.cache/artouste/` sous Linux, `%LOCALAPPDATA%\artouste\` sous Windows).
Comptez une trentaine de secondes sur une carte fine, avec une barre de
progression ; les lancements suivants sont immédiats.

Ce cache est jetable : le supprimer ne fait perdre que ce temps de préparation.
Il se refait tout seul si une carte est remplacée.

### Gestionnaire de cartes

Le menu de démarrage ouvre un écran des cartes, par le bouton `Cartes` ou par la
touche C. Il montre ce que chaque carte occupe sur le disque, permet d'allumer ou
d'éteindre ses arbres, ses bâtiments et ses tuiles, et surtout de télécharger la
haute résolution d'une carte, ou de récupérer la place qu'elle occupe.

**Ce que le téléchargement apporte.** Une carte est livrée avec une photo
aérienne d'ensemble. Elle suffit en vol haut et devient floue quand on descend.
Les tuiles sont la même photo en beaucoup plus fin. Une fois téléchargées, le sol
redevient net au ras du sol : on distingue les marquages de piste, les toits, les
voitures. Rien ne change en altitude, et rien ne se perd si vous les supprimez.

**LR et HR** sont les deux mots que l'écran emploie pour la finesse du sol :

| État | Ce que c'est | Poids | Le sol vu de près |
|------|--------------|-------|-------------------|
| **LR** | la carte telle qu'elle est livrée | 5 à 20 Mo | flou |
| **HR** | la même, plus ses tuiles de détail | 0,8 à 37 Go | net |

Une carte reste annoncée LR tant que ses tuiles n'apportent rien de visible,
même si elles occupent le disque : c'est le rapport entre leur finesse et celle
de la photo d'ensemble qui compte, jamais leur poids.

**Les tuiles ne se téléchargent qu'auprès de l'IGN**, depuis cet écran : aucune
release n'en porte, et il n'existe pas de zip à récupérer. Elles sont fabriquées
sur votre machine à partir de la BD ORTHO (Licence Ouverte Etalab 2.0), à
0,25 m/px, ce qui demande d'une dizaine de minutes sur une petite carte à
quelques heures sur une grande.

Rien ne part sans annonce : avant de télécharger, l'écran donne la place occupée,
le volume à recevoir, la durée probable et ce qui restera sur le disque visé. Il
choisit seul la finesse, adaptée à chaque carte, et ne propose pas le
téléchargement quand la photo d'ensemble est déjà assez fine pour que les tuiles
ne changent rien (voir [docs/CARTES.md](docs/CARTES.md)).

### Tuiles de détail

Une orthophoto d'un seul tenant occupe la mémoire vidéo en proportion de
l'emprise de la carte, et c'est ce budget qui plafonne la finesse au sol : sur
une grande carte comme le bassin d'Arcachon, le sol vu d'en bas reste flou.

Les cartes qui livrent un dossier `tuiles/` échappent à cette limite. Leur
orthophoto fine y est découpée en carrés de 512 pixels, dont seuls ceux qui
entourent l'appareil sont tenus en mémoire vidéo, les autres arrivant du disque
au fil du vol. La mémoire occupée devient constante, 89 Mo, quelle que soit la
taille de la carte, et le sol proche passe de plusieurs mètres par pixel à
moins d'un mètre.

Le mécanisme se règle par la clé `tuiles_fenetre_px` de `assets/config.txt` :

| Valeur | Mémoire vidéo | Rayon de détail à 0,25 m/px |
|--------|---------------|------------------------------|
| `8192` (défaut) | 89 Mo | 0,9 km |
| `16384` | 356 Mo | 1,8 km |
| `4096` | 22 Mo | 0,5 km |
| `0` | aucune | pas de détail fin |

Plus les tuiles sont fines, plus la fenêtre couvre un rayon court à mémoire
vidéo constante : au-delà, le sol revient à l'orthophoto d'ensemble, le passage
se faisant en fondu. Monter à `16384` retrouve le rayon d'avant, au prix de
356 Mo de mémoire vidéo.

Les tuiles peuvent vivre ailleurs que dans le jeu, ce qui est commode quand le
disque système est à l'étroit : la variable d'environnement `ARTOUSTE_TUILES`
désigne alors un dossier contenant un sous-dossier par carte.

```bash
ARTOUSTE_TUILES=/media/disque/tuiles ./artouste
```

Une carte sans tuiles, un dossier absent ou une tuile manquante ne posent pas
de problème : le jeu retombe sur l'orthophoto d'ensemble, comme avant.

## Contributions

Le ciel réaliste et la vue d'orbite solaire proviennent d'une contribution de
[CHAT-DISPARU](https://github.com/CHAT-DISPARU).

## Licence

Ce projet est distribué sous licence **GPL v2** (voir `LICENSE`), comme le
modèle 3D, les sons et les modèles 3D des monuments de Paris d'Emmanuel Baranger.
