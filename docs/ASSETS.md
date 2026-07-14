# Assets : modèle 3D, sons et textures

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

## Livrées du fuselage (régénérer les textures)

La touche `L` (ou le bouton `A`) fait défiler quatre livrées : blanche,
Gendarmerie (bleu), armée de terre (olive) et Protection civile (rouge). Chaque
livrée peinte est produite à partir de l'atlas d'origine `texture.png` en
reteintant les pixels neutres vers la couleur cible, sans toucher aux marquages
saturés (cocardes tricolores). Les textures sont versionnées ; régénérer ne sert
qu'après modification :

```bash
python3 tools/livree/make_blanche.py           # -> texture-blanche.png (blanc)
python3 tools/livree/make_gendarmerie.py       # -> texture-gendarmerie.png (bleu)
python3 tools/livree/make_armeedeterre.py      # -> texture-armeedeterre.png (olive)
python3 tools/livree/make_protectioncivile.py  # -> texture-protectioncivile.png (rouge)
python3 tools/livree/make_decals.py            # -> décalques (textes et immatriculations)
```
