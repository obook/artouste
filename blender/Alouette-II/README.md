# Sources Blender du modèle

Un fichier `.blend` par fichier `.ac` de `assets/models/Alouette-II/Models/`,
pour pouvoir retoucher la géométrie sans passer par AC3D.

| Fichier | Source |
|---|---|
| `alouette.blend` | la cellule complète |
| `interior.blend`, `panel.blend`, `pilot.blend` | cabine, planche de bord, pilote |
| `mainrotor.blend`, `mainrotor-blade.blend` | rotor principal : moyeu et pale |
| `tailrotor.blend`, `tailrotor-blade.blend` | rotor de queue : moyeu et pale |
| `tailguard.blend` | arceau de protection |
| `instrument-*.blend` | les onze cadrans et commandes |

Les textures sont référencées en chemin relatif vers `assets/`, donc les fichiers
restent légers mais doivent rester à cet emplacement.

## Aller-retour avec le .ac

L'import et l'export passent par l'add-on `io_scene_ac3d`, à installer dans
Blender. L'aller-retour a été vérifié sans perte sur `alouette.ac` : mêmes 93
objets, 16 370 faces, 17 738 sommets, 4 matériaux, et écart nul sur les
coordonnées.

Le moteur lit les `.ac`, jamais les `.blend` : après une retouche, il faut
réexporter vers `assets/models/Alouette-II/Models/`.
