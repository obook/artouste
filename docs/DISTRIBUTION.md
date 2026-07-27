# Distribution du jeu et de ses cartes

Proposition d'architecture pour la diffusion d'Artouste : ce que contient le
paquet initial, comment un joueur améliore la finesse de ses cartes, comment il
en ajoute d'autres, et quel outil fabrique tout cela.

Document de travail : il décrit une cible, pas l'état actuel du dépôt.

## Le problème

Le tuilage des orthophotos (voir [CARTES.md](CARTES.md)) a levé la contrainte de
mémoire vidéo : une carte peut désormais être aussi fine qu'on veut sans coûter
un octet de plus au rendu. Il a en revanche déplacé le problème sur le disque et
sur la bande passante.

Ordres de grandeur mesurés sur Ossau, 319 km² :

| Étage | Finesse | Poids |
|-------|---------|-------|
| Orthophoto d'ensemble livrée | 3,6 m/px | 5,9 Mo |
| Niveau de tuiles large | 0,75 m/px | 713 Mo |
| Niveau de tuiles serré, abords des posers | 0,20 m/px | ~1,1 Go |

Multiplié par dix cartes, cela fait une quinzaine de gigaoctets. Personne ne
télécharge quinze gigaoctets pour un simulateur de 45 Mo, GitHub plafonne un
fichier de release à 2 Go, et l'immense majorité des joueurs ne survolera que
deux ou trois cartes.

## Le principe

**Le dépôt et la release ne portent que du code et un socle minimal. Toute
donnée fine est fabriquée chez le joueur, à la demande, depuis les services de
l'IGN.**

Ce n'est pas un contournement : les données IGN (RGE ALTI, BD ORTHO, BD TOPO)
sont sous Licence Ouverte Etalab 2.0. Chacun a le droit de les récupérer, et le
jeu le fait déjà, mais aujourd'hui seulement du côté de l'auteur, dans des
scripts Python. Il s'agit de mettre cette capacité entre les mains du joueur.

Trois conséquences heureuses :

- le téléchargement du jeu reste petit et stable dans le temps ;
- chacun choisit la finesse que son disque et sa carte graphique supportent ;
- ajouter une carte ne demande plus une release, seulement une entrée de
  catalogue.

## LR et HR : le vocabulaire du gestionnaire

Pour le joueur, une carte existe dans deux états, et l'interface ne parle que de
ceux-là :

- **LR** (basse résolution) : la carte telle qu'elle est livrée. Relief de base,
  orthophoto d'ensemble. Quelques mégaoctets, jouable tout de suite, correcte en
  survol et floue au ras du sol.
- **HR** (haute résolution) : la même carte augmentée de ses tuiles de détail et,
  le cas échéant, d'un relief plus fin. Quelques centaines de mégaoctets à
  plusieurs gigaoctets.

Le joueur y gagne un sol qui reste net quand il descend. Rien d'autre ne change,
ni le relief, ni les bâtiments, ni le contenu de la carte.

HR n'est pas une question de poids. Une carte porte l'état HR si ses tuiles sont
plus fines que son orthophoto d'ensemble, et elle reste annoncée LR si elles ne
le sont pas, ses gigaoctets fussent-ils bien là : le moteur les écarte alors au
chargement, et l'écran ne doit pas promettre ce que le vol ne montre pas. C'est
pourquoi la finesse visée se calcule carte par carte, à partir de l'orthophoto de
chacune, au lieu d'être la même partout (voir `docs/CARTES.md`).

Règle d'interface, non négociable : **toute action qui remplit le disque annonce
son coût AVANT de commencer**, et le gestionnaire affiche en permanence la place
occupée par chaque carte et la place restante sur le disque. Personne ne doit
découvrir après coup qu'un simulateur de 45 Mo lui a pris six gigaoctets. Le
passage HR vers LR (suppression des tuiles) doit être aussi accessible et aussi
immédiat que l'inverse.

L'annonce porte sur trois chiffres, avant de lancer quoi que ce soit :

- **place occupée** sur le disque, une fois la carte fabriquée ;
- **durée de téléchargement**, mesurée et non devinée : l'outil chronomètre les
  premiers blocs réellement reçus et en déduit le débit, plutôt que de supposer
  une vitesse de ligne. Tant qu'il n'a pas assez de mesures, il annonce une
  fourchette, jamais un chiffre faux ;
- **place restante** après l'opération, avec un refus net si elle passait sous
  une marge de sécurité.

L'estimation se réaffiche pendant le travail, corrigée par le débit observé, avec
le nombre de blocs restants. Une fabrication de carte fine dure des dizaines de
minutes : le joueur doit pouvoir décider en connaissance de cause, et suivre
l'avancement sans se demander si le programme est bloqué.

## Les quatre étages d'une carte

Sous le vocabulaire LR/HR, une carte n'est plus un bloc mais un empilement,
chaque étage facultatif et remplaçable :

| Étage | Contenu | Livré ? | Poids typique | État |
|-------|---------|---------|---------------|------|
| 1. Socle | `terrain.txt`, relief, ortho d'ensemble, lieux, hélisurfaces | oui pour 3 cartes | 5 à 20 Mo | LR |
| 2. Bâtiments | `buildings.bin` (BD TOPO extrudée) | avec le socle | 0,7 à 9 Mo | LR |
| 3. Tuiles, niveau large | ortho fine sur toute l'emprise, 0,25 m/px en général | non | 0,1 à 10 Go | HR |
| 4. Tuiles, niveau serré | ortho à 0,20 m/px autour des posers | non | 0,3 à 1,5 Go | HR |

Le relief traverse cet empilement : sa finesse est fixée à la fabrication du
socle (nombre de points de la grille), et le tuilage ne l'améliore pas. C'est le
point traité plus bas.

## Le paquet initial

Objectif : rester sous 60 Mo, et être jouable et beau sans rien télécharger de
plus.

Composition actuelle, 45 Mo compressés :

| Poste | Poids |
|-------|-------|
| Modèles 3D (Alouette, zombies, explosion) | 32 Mo |
| Sons | 9,1 Mo |
| Trois cartes (ossau, cote-landes, dax-arene) | 46 Mo |
| Exécutable | 11 Mo |
| Textures, végétation, shaders | 1,8 Mo |

Rien à changer : c'est déjà un paquet raisonnable, et le rogner dégraderait
l'expérience de celui qui ne télécharge jamais rien. La seule évolution
proposée est de **ne plus jamais l'augmenter** : toute carte ajoutée au projet
part au catalogue, pas dans l'archive.

## Améliorer une carte existante

C'est le cas d'usage le plus fréquent, et il se traite en deux gestes
indépendants.

**L'orthophoto** : ajouter un ou deux niveaux de tuiles. Fait des deux côtés,
par les scripts de l'auteur (`tools/terrain/fetch_tuiles.py`) et par le
gestionnaire de cartes du jeu, qui télécharge et compresse lui-même.

**Le relief** : régénérer la grille d'altitudes plus fine. C'est un remplacement
en place, sans effet de bord : toutes les données annexes d'une carte (lieux,
hélisurfaces, balises, bâtiments, points de spawn) sont en longitude-latitude ou
en coordonnées monde, jamais en indices de grille. Doubler la finesse du relief
ne demande donc de retoucher aucun autre fichier.

Ce que cela coûte, en revanche, c'est du sommet à dessiner :

| Grille | Maille sur Ossau | Sommets |
|--------|------------------|---------|
| 512 | 35 m | 0,26 M |
| 1024 (actuel) | 17,5 m | 1,0 M |
| 2048 | 8,8 m | 4,2 M |
| 4096 | 4,4 m | 16,8 M |

Ces sommets ne sont toutefois plus une fatalité : depuis le 27/07/2026, la
finesse de la DONNÉE est découplée de celle du MAILLAGE. La clé
`relief_sommets_max` plafonne le nombre de points dessinés, le moteur n'en
retenant qu'un sur deux, sur trois... au besoin, tandis que `heightAt` continue
de lire toutes les altitudes. Une carte en grille 2048 coûte donc exactement le
même rendu qu'avant sur une machine modeste, tout en posant l'appareil, portant
les hélisurfaces et calculant la collision sur un relief quatre fois plus fin.

Au-delà de 2048, ou pour dessiner réellement cette finesse, il faudra un
maillage par morceaux avec niveaux de détail, c'est-à-dire pour le relief ce que
le tuilage a fait pour la texture. Chantier à part, non couvert ici.

**Bonne nouvelle mesurée le 27/07/2026** : la Géoplateforme sert le relief en
RASTER, couche `ELEVATION.ELEVATIONGRIDCOVERAGE.HIGHRES`, format
`image/x-bil;bits=32`, soit un tableau de flottants 32 bits. Vérifié sur le pic
du Midi d'Ossau : une requête de 64 x 64 rend 16 384 octets d'altitudes
cohérentes. Aujourd'hui `tools/terrain/relief.py` interroge l'API JSON point par
point, 200 au maximum par requête, soit 1310 requêtes pour une grille 512 et
plus de 5000 pour une grille 1024. Passer au raster, c'est **quelques requêtes au
lieu de quelques milliers**, des secondes au lieu de longues minutes, et un
relief fin qui devient abordable.

## Ajouter une carte

Une carte se définit par presque rien : un nom, une emprise géographique, une
finesse de relief, une finesse d'ortho, un point de départ. C'est déjà ce que
contient une entrée de `tools/terrain/zones/`.

Proposition : publier ce catalogue sous forme de fichier texte livré avec le jeu
(`assets/zones.txt`), une ligne par zone, que l'outil sait fabriquer. Le joueur
peut y ajouter la sienne sans écrire une ligne de Python, et l'auteur enrichit le
catalogue sans faire de release.

```
# nom          lon_min   lat_min   lon_max   lat_max   grille  ortho_px  titre
biarritz      -1.5900   43.4500   -1.5000   43.5100   1024    6000      Biarritz, la Côte des Basques
```

Les lieux remarquables et les hélisurfaces resteraient, eux, du travail
d'auteur : ils demandent un repérage humain (voir la campagne de vérification
des positions, mémoire du projet). Une carte fabriquée par un joueur naîtrait
donc sans étiquettes ni pads, sauf son point de départ.

## L'outil

Trois formes possibles, par ordre de coût.

**A. Garder les scripts Python, côté auteur seulement.** Zéro travail. Le joueur
ne fabrique rien, il télécharge des zips préfabriqués. C'est la situation
actuelle, et elle bute sur les quinze gigaoctets.

**B. Un exécutable dédié, livré à côté du jeu** (`artouste-cartes`). C'est la
proposition. Il réutilise telles quelles les briques du moteur : libcurl (déjà
présente pour la radio), stb_image pour le JPEG, `render/Bc7` et `render/Dds`
pour la compression, `render/tuiles` pour la grille. Il ne dépend ni de Python,
ni de numpy, ni de scipy, et se compile pour Linux et Windows avec le reste.

**C. Intégré au menu du jeu**, écran "Cartes" avec barre de progression. La plus
agréable pour le joueur, mais elle mêle réseau et boucle de rendu, et rend le
jeu responsable d'échecs qui ne le regardent pas. À faire après B, en lançant
simplement l'outil depuis le menu.

### Arbres et bâtiments, carte par carte

Le gestionnaire doit permettre d'activer ou de désactiver les arbres et les
bâtiments. Les deux se ressemblent à l'écran mais n'ont rien de commun dessous,
et l'interface doit le refléter :

- **Les bâtiments sont de la DONNÉE** (`buildings.bin`, BD TOPO extrudée, de
  0,7 à 9 Mo selon la carte). Les désactiver, c'est pouvoir les effacer et
  récupérer la place ; les activer, c'est un téléchargement. Ils ont donc leur
  taille affichée, comme les tuiles, et rejoignent la logique LR/HR.
- **Les arbres ne sont AUCUNE donnée** : ils sont semés à la volée d'après
  l'orthophoto, à chaque chargement. Les activer ne coûte pas un octet de
  disque, seulement des images par seconde -- c'est le poste de rendu le plus
  cher du moteur. Leur case ne doit donc afficher aucune taille, mais un
  avertissement de performance.

Le réglage est PAR CARTE, et non global comme aujourd'hui : les arbres comptent
en montagne et les bâtiments en ville, rarement les deux au même endroit. Chaque
carte porte donc un `options.txt` facultatif, du même genre que ses autres
fichiers optionnels (`zombies.txt`, `hapi.txt`, `exclusions.txt`) :

```
# Options de la carte, écrites par le gestionnaire de cartes.
arbres 1
batiments 0
```

Absent, ou clé absente, la configuration générale s'applique : une carte livrée
sans ce fichier se comporte exactement comme avant.

Ce que l'outil sait faire, dans l'ordre de mise en oeuvre :

1. lister le catalogue avec, pour chaque carte, ce qui est déjà sur le disque et
   ce que coûterait chaque étage ;
2. fabriquer un socle : relief par raster BIL, ortho par WMS, écriture de
   `terrain.txt` ;
3. fabriquer les niveaux de tuiles, avec reprise après interruption ;
4. supprimer un étage pour récupérer de la place ;
5. vérifier et réparer (tuile manquante, index incohérent).

Deux points techniques à trancher à l'écriture :

- **Format du relief.** `stb_image_write` n'écrit pas le PNG 16 bits, et
  embarquer zlib pour cela seul serait disproportionné. Le plus simple est
  d'accepter, à côté de `heightmap.png`, un `heightmap.bin` brut en entiers
  16 bits, que l'outil écrit et que le moteur lit. Une trentaine de lignes dans
  `Terrain.cpp`.
- **Bâtiments.** `fetch_buildings.py` interroge un service WFS et analyse du
  GeoJSON ; le porter demanderait un lecteur JSON. À laisser côté auteur dans un
  premier temps : une carte fabriquée par un joueur naît sans bâtiments, ce qui
  se voit peu en montagne et beaucoup en ville.

## Ce que devient la release

| Fichier | Poids | Rôle |
|---------|-------|------|
| `artouste-Linux-x86_64.tar.gz` | ~50 Mo | jeu, trois cartes, outil de cartes |
| `artouste-Windows-AMD64.zip` | ~50 Mo | idem |
| `carte-<nom>.zip` x7 | 5 à 22 Mo | socles des cartes existantes |
| `tuiles-ossau.zip` | 655 Mo | vitrine : la carte phare, prête à l'emploi |

Tout le reste se fabrique. Les zips de socles restent parce qu'ils sont légers et
qu'ils portent le travail d'auteur (lieux, pads, bâtiments) qu'une fabrication
locale ne saurait pas reproduire.

## Étapes proposées

| # | Étape | Effort | Gain |
|---|-------|--------|------|
| 1 | ~~Relief par raster BIL dans le pipeline Python~~ FAIT le 27/07/2026 | | 4 requêtes et 57 s au lieu de 1310 requêtes et plusieurs minutes |
| 2 | ~~Relief en grille 2048 sur les cartes de montagne~~ FAIT pour Ossau | | maille de 8,8 m, sans coût de rendu grâce au budget de sommets |
| 3 | `heightmap.bin` lu par le moteur | 2 h | prérequis de l'outil C++ |
| 4a | ~~Écran "Cartes" dans le menu : inventaire, tailles, arbres/bâtiments/tuiles par carte, suppression~~ FAIT | | le joueur voit et défait |
| 4b | ~~Fabrication des tuiles depuis l'écran (LR vers HR)~~ FAIT | | le joueur fabrique, sans Python |
| 4c | Fabrication d'une carte NEUVE (relief + ortho + calage) | 1 à 2 j | le joueur ajoute une carte |
| 5 | Catalogue `assets/zones.txt` + documentation | 1/2 j | ajouter une carte sans release |
| 6 | Bouton "Cartes" dans le menu | 1 j | confort |

Les étapes 1 et 2 valent d'être faites même si le reste est abandonné : elles
n'engagent rien et améliorent les cartes existantes tout de suite.

## Risques

- **Charge sur les serveurs de l'IGN.** Une fabrication de carte fine, c'est des
  centaines de requêtes. L'outil doit s'espacer, réessayer proprement sur 429, et
  ne jamais paralléliser à outrance. Le service est public et gratuit, il ne
  faut pas en abuser.
- **Attribution.** La Licence Ouverte impose de citer la source. Elle l'est déjà
  dans `CREDITS.md` et dans les en-têtes de `terrain.txt` ; une carte fabriquée
  localement doit hériter de la même mention.
- **Disponibilité du service.** Un joueur hors ligne ne fabrique rien. D'où
  l'importance de garder un paquet initial autonome et jouable.
- **Attentes.** Fabriquer une carte fine prend des dizaines de minutes. L'outil
  doit l'annoncer avant de commencer, avec une estimation de temps et de disque.
