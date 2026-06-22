# Modèle Alouette II - modifications pour les livrées

Ce document récapitule toutes les modifications apportées au modèle FlightGear de
l'Alouette II (paquet d'Emmanuel Baranger, helijah, sous GPL) pour obtenir une
livrée correcte, notamment la livrée Gendarmerie nationale. Il sert de référence
pour **ajouter d'autres livrées plus tard**.

Auteur : O. Booklage
Licence : GPL v2

## Vue d'ensemble

Le rotor de queue est l'élément le plus marqué d'une livrée Gendarmerie :

- **pales** jaunes à zébrures rouges ;
- **arceau de protection** jaune ;
- poutre et fuselage bleus.

En livrée d'origine, ces éléments sont au naturel : pales en métal nu, arceau gris
métal, fuselage clair.

Deux chantiers ont été menés : les pales (texture peinte) et l'arceau (chirurgie
du modèle). Tous deux se commutent avec la livrée, comme le fuselage.

## 1. Pales du rotor de queue (texture peinte)

### Problème

Les pales (`Externals/TailRotor/blade.ac`, objet `blade`) échantillonnent le quart
**haut-droit** de `Externals/TailRotor/tailrotor.png` (UV U[0.53,1.0] V[0.53,1.0]),
zone qui était **noire, non peinte** : les pales s'affichaient en noir plat.

### Outil

`tools/livree/make_tailrotor.py` (script Blender headless) :

1. importe `blade.ac` et lit ses triangles (coordonnées UV + position d'envergure
   X locale de chaque sommet) ;
2. rastérise lui-même les triangles de la pale (NumPy) sur une copie de la texture,
   en coloriant chaque pixel selon sa **position d'envergure** (et non selon les axes
   UV, car l'îlot UV de la pale est de travers) ;
3. gère la conversion sRGB -> linéaire pour écrire la bonne teinte.

Il produit deux textures (le moyeu et le disque sont préservés au pixel près) :

- `tailrotor.png` : pales **métal nu** (gris, livrée d'origine) ;
- `tailrotor-gendarmerie.png` : pales **jaunes à zébrures rouges** (Gendarmerie).

Lancement : `blender --background --python tools/livree/make_tailrotor.py`
Contrôle : `tools/.venv/bin/python tools/livree/check_tailrotor.py <png> [--zebra]`

### Couleurs de référence (telles que rendues, sRGB)

- jaune des pales : **(232, 154, 1)** ;
- gris métal des pales : **(100, 104, 111)**.

## 2. Arceau de protection (chirurgie du modèle)

### Problème

L'arceau n'est **pas un objet séparé** : il est fondu dans l'objet `structure` de
`alouette.ac` (la poutre tubulaire). Sa pièce connexe (353 sommets) contient l'arceau
en D **plus un tube structurel** qui court vers l'avant. Ses UV chevauchent celles de
la poutre : impossible de le recolorer par la seule texture (on jaunirait d'autres
tubes). Il a donc fallu l'isoler géométriquement.

### Chirurgie (Blender)

`tools/livree/make_tailguard.py` :

1. importe `alouette.ac` ;
2. **charge `texture.png` dans les matériaux** (voir le piège ci-dessous) ;
3. sépare l'objet `structure` en parties connexes, repère la pièce de l'arceau
   (Xmax > 4, mince en Z, descend bas en Y) ;
4. dans cette pièce, sépare les faces de la **D-hoop** (centre de face en X > 2.8) ;
5. exporte la D-hoop seule vers `tailguard.ac` ;
6. exporte le reste vers `alouette.ac` (sans l'arceau).

Résultat : l'arceau bleu d'origine disparaît du fuselage, et la D-hoop devient une
**pièce séparée** rechargée par le moteur, donc commutable indépendamment.

> Piège majeur de l'export AC3D : à l'import, le greffon garde le **nom** de la
> texture mais ne **charge pas l'image**. Sans image, l'export réécrit les matériaux
> **sans texture** : tout le modèle devient non texturé (le fuselage rend en gris et
> la livrée ne s'applique plus). Il faut donc charger `texture.png` dans les noeuds
> Image Texture des matériaux **avant** d'exporter. Vérification : `model_probe` doit
> montrer `tex:texture.png` et non `tex:(aucune)`.

> Conséquence : `alouette.ac` est désormais un **aller-retour Blender** (précision
> de sommets réduite, fichier ~27% plus petit, visuellement identique). L'original
> reste dans l'historique git.

### Couleur de l'arceau

L'arceau utilise des textures **unies** commutées par la livrée (sa texture de base
a des UV chaudes, inexploitables) :

- `tailguard-gendarmerie.png` : **jaune (232, 154, 1)** (= jaune des pales) ;
- `tailguard-origine.png` : **gris métal (100, 104, 111)** (= métal des pales).

## 3. Mécanisme de commutation de livrée (C++)

Tout passe par `render::LoadedHelicopter::setGendarmerieLivery(bool on)`, qui
remplace la texture (via `Model::setLivery`) de chaque pièce concernée :

- `m_fuselage`  -> `texture-gendarmerie.png` (bleu) ou texture d'origine ;
- `m_tailBlade` -> `tailrotor-gendarmerie.png` (zébré) ou `tailrotor.png` (métal) ;
- `m_tailGuard` -> `tailguard-gendarmerie.png` (jaune) ou `tailguard-origine.png` (gris).

La bascule est déclenchée par la touche **L** (ou le bouton **A** de la manette),
état par défaut : Gendarmerie. L'arceau est chargé depuis `tailguard.ac` et dessiné
dans `drawAirframe` (`LoadedHelicopterDraw.cpp`).

## 4. Chaîne Blender

- Blender 4.0.2 + greffon **`io_scene_ac3d`** (fork NikolaiVChr) installé dans
  `~/.config/blender/4.0/scripts/addons/`.
- Le greffon cible Blender 4.3 ; correctif local pour Blender < 4.1 (garde
  `set_sharp_from_angle` dans `import_ac3d.py`, voir README).
- Piège export texture : charger l'image avant d'exporter (section 2).

Les `.ac` et leurs textures étant versionnés, compiler et lancer le simulateur ne
demandent ni Blender ni greffon.

## 5. Limites connues

- `alouette.ac` est un aller-retour Blender (précision réduite, sans effet visible).
- Un court **segment du tube avant** (partie de la pièce d'arceau laissée dans
  `structure`) garde les UV chaudes : il apparaît légèrement **orangé** en livrée
  d'origine, près de la jonction de l'arceau. Mineur, laissé tel quel.

## 6. Ajouter une nouvelle livrée

Pour une livrée `<nom>`, prévoir les variantes de texture :

- fuselage : `Models/texture-<nom>.png` ;
- pales de queue : `Externals/TailRotor/tailrotor-<nom>.png` (générée par
  `make_tailrotor.py` en y ajoutant la fonction de couleur voulue) ;
- arceau : `Models/tailguard-<nom>.png` (couleur unie ; souvent jaune aussi).

Puis étendre la commutation. Aujourd'hui c'est une **bascule binaire**
(`setGendarmerieLivery(bool)`). Pour plusieurs livrées, prévoir un vrai **système
piloté par données** : un identifiant de livrée (enum ou clé), chacun portant son
jeu de textures (fuselage, pales, arceau), et une méthode `setLivery(id)` qui
applique le jeu correspondant à chaque pièce. C'est une évolution à concevoir à part.
