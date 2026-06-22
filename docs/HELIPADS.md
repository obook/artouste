# HELIPADS.md - Ajouter des hélipads à une carte

Les hélipads sont affichés dans le HUD comme points de navigation : une étiquette
projetée sur la scène 3D et un point sur la minimap. Ce document explique
**comment en ajouter à une carte** (terrain) du simulateur.

Aucune donnée n'est stockée ici : les hélipads viennent d'OpenStreetMap (que l'on
interroge a la demande) et la seule source de vérité dans le dépôt est le champ
`helipads` de chaque zone, dans `tools/fetch_terrain.py`. On évite ainsi toute
liste recopiée qui se périmerait.

---

## Le chemin dans le jeu (source de vérité)

Il n'y a aucun tableau d'hélipads codé en dur dans `Application.cpp`. Le chemin réel :

1. Chaque carte déclare ses hélipads dans le champ `helipads` de son entrée du
   dictionnaire `ZONES` (`tools/fetch_terrain.py`), sous la forme `("Nom", lon, lat)`.
2. `python3 tools/fetch_terrain.py <zone>` écrit `assets/terrain/<zone>/helipads.txt`
   (une ligne `lon lat nom` par hélipad).
3. Au lancement, `render::Terrain` lit ce fichier ; `Application::buildNavHud()`
   projette chaque `m_terrain->helipads()` comme étiquette sur la scène et comme
   point sur la minimap (tous étiquetés "Hélisurface").

---

## Procédure pour alimenter une zone

1. **Relever la bbox de la zone** dans `ZONES` (`tools/fetch_terrain.py`). Elle est
   au format `(lon_min, lon_max, lat_min, lat_max)`.

2. **Interroger OpenStreetMap** sur cette emprise, dans Overpass Turbo
   (<https://overpass-turbo.eu>). Attention : Overpass attend l'ordre
   `(lat_min, lon_min, lat_max, lon_max)` (latitude d'abord).

   ```
   [out:csv(name, ::lat, ::lon)][timeout:60];
   (
     node["aeroway"~"helipad|heliport"](lat_min,lon_min,lat_max,lon_max);
     way ["aeroway"~"helipad|heliport"](lat_min,lon_min,lat_max,lon_max);
   );
   out center;
   ```

   Lancer la requête (les points doivent apparaitre sur la carte), puis
   "Exporter > Données". La sortie est un texte tabulé : colonnes `name`, `@lat`,
   `@lon`. L'enregistrer dans un fichier temporaire, par exemple `/tmp/helipads.tsv`.

3. **Convertir en tuples** `("Nom", lon, lat)` prêts à coller. Les points sans nom
   (la majorité) reçoivent un libellé générique :

   ```bash
   python3 - /tmp/helipads.tsv <<'PY'
   import csv, sys
   with open(sys.argv[1], encoding="utf-8") as f:
       for ligne in csv.DictReader(f, delimiter="\t"):
           nom = ligne["name"].strip() or "Hélipad"
           print(f'            ("{nom}", {float(ligne["@lon"]):.4f}, {float(ligne["@lat"]):.4f}),')
   PY
   ```

4. **Coller le résultat** dans le champ `helipads` de la zone (créer le champ s'il
   n'existe pas encore).

5. **Nettoyer** :
   - dédoublonner les points quasi-identiques (un même site est parfois saisi
     plusieurs fois dans OSM) ;
   - écarter ceux qui tombent hors du relief réel (une bbox rectangulaire déborde
     du terrain) et ceux qui font doublon avec un hélipad déjà curé a la main ;
   - retirer les points hors du pays voulu si l'emprise chevauche une frontière.

6. **Régénérer** : `python3 tools/fetch_terrain.py <zone>` réécrit `helipads.txt`.
   (On peut aussi éditer `helipads.txt` directement, au même format `lon lat nom`.)

---

## Source et licence des données

Les hélipads proviennent d'**OpenStreetMap**, interrogé via **Overpass Turbo**
(<https://overpass-turbo.eu/>). La donnée n'est donc pas figée dans le dépôt :
elle se régénère a la demande en relançant la requête de l'étape 2 ci-dessus.

Licence : **ODbL** (Open Database Licence). Mentionner "(c) OpenStreetMap
contributors" (<https://www.openstreetmap.org/copyright>) dans les crédits du projet.
