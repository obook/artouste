# -*- coding: utf-8 -*-
"""
Paquet du téléchargeur de terrain Artouste (données IGN Géoplateforme).

Découpé en modules pour rester lisible :
  - zones   : description des zones disponibles (emprise, lieux, hélipads) ;
  - config  : services IGN, paramètres de grille et réglages de la zone choisie ;
  - relief  : téléchargement du relief (RGE ALTI) et calage du point de départ ;
  - ortho   : téléchargement et nettoyage de l'orthophoto (BD ORTHO) ;
  - outputs : écriture des fichiers lus par le moteur (calage, lieux, hélipads).

Le script d'entrée est tools/fetch_terrain.py.

Auteur : O. Booklage
Licence : GPL v2
"""
