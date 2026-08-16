"""
lidar
Outils de fabrication de modèles 3D à partir du LiDAR HD de l'IGN.

L'IGN publie les produits dérivés de son relevé laser (sol nu, surface, hauteur
de sursol) en grilles raster, servies par le même service WMS que le relief des
cartes : aucun nuage de points, aucun outil tiers.

  services  accès aux couches IGN
  carte     lien avec la carte du simulateur : relief, recalage, raccord
  bati      distinction bâti/rocher, nettoyage, coupoles
  maillage  champ de hauteurs vers maillage texturé
  gltf      écriture du modèle au format que le moteur sait lire

Entrées : tools/observatoire.py (monument du Pic du Midi).

Auteur : O. Booklage
Licence : GPL v2
"""
