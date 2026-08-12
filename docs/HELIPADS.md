# HELIPADS.md - Ajouter des hélipads à une carte

Les hélipads sont affichés dans le HUD comme points de navigation : une étiquette
projetée sur la scène 3D et un point sur la minimap.

La seule source de vérité dans le dépôt est le champ `helipads` de chaque zone,
dans `tools/terrain/zones/<zone>.py`, sous la forme `("Nom", lon, lat)`.

---

## Le chemin dans le jeu

1. La zone déclare ses hélipads dans son champ `helipads`.
2. `python3 tools/fetch_terrain.py <zone>` écrit `assets/terrain/<zone>/helipads.txt`
   (une ligne `lon lat nom`).
3. Au lancement, `render::Terrain` lit ce fichier et `Application::buildNavHud()`
   projette chaque point comme étiquette sur la scène et sur la minimap.

Le fichier livré accepte aussi un cap facultatif, `lon lat cap nom`, qui oriente
le H au sol. Le générateur ne l'écrit pas : un cap saisi à la main dans
`helipads.txt` est perdu si la zone est régénérée.

---

## Les deux listes officielles

À recouper avant d'ajouter quoi que ce soit.

**IGN BD TOPO, couche `aerodrome`** : 1 370 objets, dont 704 de nature
`Héliport`. C'est la liste de référence. Licence Ouverte Etalab 2.0.

```bash
curl -s "https://data.geopf.fr/wfs/ows?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature\
&TYPENAMES=BDTOPO_V3:aerodrome&COUNT=1000&STARTINDEX=0\
&OUTPUTFORMAT=application/json&SRSNAME=EPSG:4326" -o aerodromes.json
```

Les objets sont des polygones : prendre le centre de l'anneau extérieur. Ne
retenir que `nature == "Héliport"`. Les natures `Aérodrome`, `Altiport` et
`Hydrobase` désignent le terrain entier, pas une aire de poser ; leur centre
tombe au milieu du champ.

**Hélistations hospitalières** (data.gouv.fr, HéliApps) : 183 lignes, colonnes
`Nom`, `Latitude`, `Longitude`.

```bash
curl -sL "https://static.data.gouv.fr/resources/helistations-hospitalieres/\
20210713-182330/helistations.csv" -o helistations.csv
```

Cette liste porte au moins une erreur de coordonnées connue (Lariboisière est
placé à La Défense) : la recouper avec la BD TOPO avant d'en tirer un point.

---

## Le filet OpenStreetMap

OSM couvre ce que les deux listes ignorent : aires privées, DZ de refuge,
plateformes d'hôpital non répertoriées. `docs/HELIPADS.tsv` est un relevé
national (`aeroway=helipad|heliport`) daté du 25/06/2026, colonnes `name`,
`aeroway`, `@lat`, `@lon`. Le rejouer dans Overpass Turbo pour le rafraîchir :

```
[out:csv(name, ::lat, ::lon)][timeout:60];
(
  node["aeroway"~"helipad|heliport"](lat_min,lon_min,lat_max,lon_max);
  way ["aeroway"~"helipad|heliport"](lat_min,lon_min,lat_max,lon_max);
);
out center;
```

Overpass attend la latitude d'abord, l'inverse des bbox de `zones/`.

---

## Nettoyage

- Dédoublonner : un même site est souvent saisi plusieurs fois dans OSM.
- Écarter les postes de stationnement. Une base d'hélicoptères en aligne une
  douzaine, tous marqués `helipad` : ce ne sont pas des aires de poser
  distinctes. Trancher sur l'orthophoto (Dax, Pau).
- Écarter les points hors relief et hors du pays quand l'emprise déborde.
- Vérifier chaque point sur l'orthophoto de la carte avant de le garder.

---

## Licences

IGN BD TOPO : Licence Ouverte Etalab 2.0. OpenStreetMap : ODbL, mentionner
"(c) OpenStreetMap contributors".
