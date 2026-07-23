# Outils Artouste (tools/)

Scripts de génération et de mise à jour des assets (terrain, livrées,
textures procédurales, végétation). Environnement Python : voir tools/.venv
(certains scripts s'exécutent sous Blender, voir leur en-tête).

## Convention d'exécution et d'import

Chaque script s'exécute par son chemin direct depuis la racine du dépôt,
par exemple :

```
python3 tools/fetch_terrain.py ossau
tools/.venv/bin/python tools/facade/generer_facade.py
blender --background --python tools/livree/make_tailrotor.py
```

Python ajoute automatiquement le dossier du script lancé à `sys.path` : un
script rangé directement dans un sous-dossier thématique (tools/livree/,
tools/vegetation/...) importe donc sans rien ajouter les modules qui vivent
à côté de lui dans le même dossier (par exemple tools/livree/retint.py
depuis tools/livree/make_gendarmerie.py).

Les paquets communs (tools/common/, tools/terrain/) sont, eux, des dossiers
FRÈRES du script appelant : pour les importer, un script ajoute le dossier
tools/ à sys.path avec une ligne unique avant ses imports locaux :

```python
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # tools/

from common import paths
```

Les scripts directement rangés dans tools/ (fetch_terrain.py,
fetch_buildings.py) sont déjà dans ce dossier : ils n'ont besoin d'aucun
ajout à sys.path pour importer tools/common/ ou tools/terrain/.

## Paquets

- `common/` : chemins du dépôt (paths.py), bruit procédural (imaging.py).
- `terrain/` : téléchargement et mise à jour des cartes IGN (voir son
  propre docstring de paquet).
- `livree/` : génération des livrées du fuselage, du pilote et des rotors.
- `vegetation/` : atlas de sprites d'arbres.
