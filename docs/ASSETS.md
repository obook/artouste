# Assets : modèle 3D, sons et textures

## Modèle 3D et sons

Le modèle 3D de l'Alouette II et les sons proviennent du paquet **FlightGear**
d'Emmanuel Baranger (helijah), sous licence GPL. Le sous-ensemble utilisé par
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

## Livrées du fuselage (régénérer les textures)

La touche `L` (ou le bouton `A`) fait défiler quatre livrées : blanche,
Gendarmerie (bleu), armée de terre (olive) et Protection civile (rouge). Chaque
livrée peinte est produite à partir de l'atlas d'origine `texture.png` en
reteintant les pixels neutres vers la couleur cible, sans toucher aux marquages
saturés (cocardes tricolores). Les textures sont versionnées ; régénérer ne sert
qu'après modification :

Les livrées sont en 2048 : chaque make_* part d'un fond fabriqué par
`make_fond.py` (atlas d'origine agrandi, lignes de tôle redessinées depuis la
carte de relief cuite), puis `retint.py` repeint, applique l'occlusion, assombrit
la turbine et pose la salissure. Ces cartes de travail sont cherchées A COTE du
fond : on travaille dans un dossier à part (ici `~/.cache/cc/livree`, le Blender
flatpak ne voit pas /tmp). Chaîne complète, depuis la racine du dépôt :

```bash
W=~/.cache/cc/livree; M=assets/models/Alouette-II/Models; mkdir -p $W
./build/bin/model_probe $M/alouette.ac $W/alouette.obj "hdr,blur,disc,flotteur,barre,roue"
./build/bin/model_probe $M/alouette.ac $W/tubes.obj "fuselage,cache,hdr,blur,disc"
cp $W/tubes.obj $W/texture-ao-epargne.obj
cp $M/cuisson/texture-ao.png $M/cuisson/texture-turbine.obj $W/
PYTHONPATH=tools/livree tools/.venv/bin/python tools/livree/make_fond.py \
    $M/texture.png $M/cuisson/texture-relief.png $W/texture-fond.png $W/tubes.obj
flatpak run org.blender.Blender -b --factory-startup \
    -P $PWD/tools/livree/cuire_geometrie.py -- $W/alouette.obj $W/geo 2048
tools/.venv/bin/python tools/livree/usure.py $W/geo $W/texture-usure.png
for l in blanche gendarmerie armeedeterre protectioncivile; do
    PYTHONPATH=tools/livree tools/.venv/bin/python tools/livree/make_$l.py \
        $W/texture-fond.png $M/texture-$l.png
done
python3 tools/livree/make_decals.py            # -> décalques (textes et immatriculations)
```

`cuisson/texture-turbine.obj` est la zone UV de la turbine (faces du maillage
"moteur" assombries à 0,10 de luminance) : il est versionné de force, `*.obj`
étant ignoré par git, parce que le fichier d'origine a été perdu une fois avec
/tmp. `cuire_geometrie.py` cuit position et normale sur l'atlas, `usure.py` en
tire la salissure en mètres (suie derrière l'échappement, coulures sur les
flancs, crasse en bas) : ses réglages sont des constantes en tête de fichier.

## Cellule de rechange en glTF (modèle 3dexport, hors dépôt)

Le modèle commercial 3dexport de l'Alouette II (archive dans le coffre
Obsidian, projets/artouste/3dmodel/) se charge à la place d'alouette.ac dès
que `assets/models/Alouette-II/Models/alouette-3dexport.glb` existe (ce
dossier est ignoré par git, le .glb reste local) ; ARTOUSTE_MODELE force un
autre fichier. Les meubles du poste du FBX (planche, consoles, palonniers,
manches, collectif) sont retirés à la conversion, le jeu pose les siens ; les
feux de poutre et de tuyère ont leurs repères propres (constantes *_GLTF dans
ApplicationRenderEffects.cpp, celles du .ac restent à côté). Conversion depuis le FBX extrait (unrar, 7z ne lit pas cette archive) :

```bash
flatpak run org.blender.Blender -b --factory-startup \
    -P tools/modele/convertir_3dexport.py -- \
    .../3dmodel/civil/Civil_FBX.FBX \
    assets/models/Alouette-II/Models/alouette-3dexport.glb
```

Seule la cellule est convertie (vitrages à part, dans un noeud "verriere") ;
rotors, pilotes, planche de bord et commandes restent ceux du .ac, posés par
le moteur. Avec une cellule glTF, le moteur n'applique ni livrée ni décalques,
et ne dessine ni l'intérieur ni l'arceau du .ac. Les patins posent à la hauteur
de ceux du .ac (y = -1,387, Y_OFFSET) et le mât, plus court sur ce modèle,
monte 16 cm au-dessus du plan des pales du jeu : le moyeu de mainrotor.ac
descend assez pour couvrir le raccord.

Livraison aux joueurs : la licence Basic de 3dexport autorise un jeu gratuit,
mais pas de fournir le modèle lui-même, ni sous une forme où l'utilisateur
final peut le récupérer. Le .glb n'entre donc ni dans ce dépôt public ni en
fichier dans les archives (exclu par `install(DIRECTORY assets/ ...)`). Il est
fondu dans l'exécutable, octets mélangés à une clé : l'option CMake
`ARTOUSTE_CELLULE_EMBARQUEE` désigne le .glb, `tools/modele/embarquer_cellule.py`
en fait un source C++, `src/render/CelluleEmbarquee.cpp` le restitue à Assimp.
Un .glb présent sur le disque garde la priorité. `release.yml` lit le fichier
dans le dépôt privé `obook/artouste-assets` avec le secret
`ARTOUSTE_ASSETS_TOKEN` (jeton en lecture seule sur ce seul dépôt).

