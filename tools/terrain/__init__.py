"""
Paquet du téléchargeur de terrain Artouste (données IGN Géoplateforme).

Découpé en modules pour rester lisible :
  - zones   : description des zones disponibles (emprise, lieux, hélipads) ;
  - config  : services IGN, paramètres de grille et réglages de la zone choisie ;
  - relief  : téléchargement du relief (RGE ALTI) et calage du point de départ ;
  - ortho   : téléchargement et nettoyage de l'orthophoto (BD ORTHO) ;
  - outputs : écriture des fichiers lus par le moteur (calage, lieux, hélipads) ;
  - meta    : lecture/mise à jour de terrain.txt (recadrage, réémission d'ortho) ;
  - grid    : conversions grille <-> monde <-> lon/lat (recadrage).

Le script d'entrée est tools/fetch_terrain.py. crop_zombie_map.py et
refresh_ortho.py, dans ce même paquet, réutilisent ortho/meta/grid pour
alléger ou recaler un terrain déjà généré.

Auteur : O. Booklage
Licence : GPL v2
"""
