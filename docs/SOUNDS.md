# SOUNDS.md - Mixage audio par vue

Le son du simulateur repose sur deux boucles : la **turbine** (`running-outside.wav`)
et le **rotor** (`rotor-outside.wav`). Leur volume et leur hauteur sont modulés en
continu selon l'état de vol, puis ajustés par un profil propre à la **vue caméra**
active (mixage, timbre, effet Doppler).

Ces valeurs vivent dans `src/audio/AudioEngine.cpp` (fonction `viewMix`). Ce tableau
en est le reflet ; le retoucher ici sert de référence pour ajuster le code.

## Tableau de mixage par vue

| Vue | Caméra | Volume turbine | Volume rotor | Timbre (tone) | Doppler |
|-----|--------|:--------------:|:------------:|:-------------:|:-------:|
| Intérieur | cockpit (solidaire) | 0,80 | 0,70 | 0,90 | non |
| Arrière   | poursuite           | 1,00 | 1,00 | 1,00 | non |
| Fly       | orbite (libre)      | 0,90 | 1,00 | 1,00 | oui |

- **Volume turbine / rotor** : facteurs multiplicateurs (le mixage entre les deux
  sources). 1,00 = volume de base inchangé.
- **Timbre (tone)** : facteur appliqué à la hauteur pour approcher une égalisation.
  Sous 1,00, le son est assombri (étouffé, comme dans la cabine). C'est une
  approximation (un vrai filtre passe-bas serait plus réaliste).
- **Doppler** : actif seulement en vue orbite, où la caméra est quasi fixe et
  l'appareil passe devant elle.

## Volumes et hauteurs de base (avant profil de vue)

Avant l'application du profil ci-dessus, chaque source est modulée ainsi :

| Source | Volume de base | Hauteur de base |
|--------|----------------|-----------------|
| Turbine | (0,35 + 0,55 x collectif) x régime_turbine | 0,55 + 0,35 x régime_turbine + 0,30 x collectif |
| Rotor   | (0,35 + 0,20 x facteur_vitesse) x régime_rotor | 1,00 |

- `régime_turbine` et `régime_rotor` : dans [0, 1] (0 = turbine coupée, 1 = plein régime).
- `collectif` : dans [0, 1].
- `facteur_vitesse` : min(1, vitesse_air / 40 m/s) ; le rotor se fait un peu plus
  entendre en translation.

Volume final = volume_de_base x volume_de_vue.
Hauteur finale = hauteur_de_base x timbre x doppler.

## Effet Doppler (vue orbite)

- Vitesse de rapprochement : vitesse propre de l'appareil projetée sur l'axe
  caméra -> appareil (la caméra d'orbite est considérée fixe). Bornée à +/- 120 m/s.
- Lissage : constante de temps 0,25 s (entrée et sortie d'effet en douceur, sans
  saut sur les changements de vue).
- Hauteur : `doppler = 340 / (340 - vitesse_rapprochement)` (340 m/s = vitesse du son).
  Rapprochement positif -> son plus aigu ; éloignement -> son plus grave.
- En vues intérieure et arrière, la caméra suit l'appareil : aucun mouvement
  relatif, donc Doppler neutre (= 1,00).

## Pistes d'amélioration

- Vraie égalisation par filtre passe-bas (graphe de noeuds miniaudio) au lieu du
  facteur de hauteur.
- Boucles "inside" dédiées pour la vue intérieure (`running-inside.wav`,
  `rotor-inside.wav` existent dans le paquet FlightGear, non versionnées).
- Spatialisation 3D native de miniaudio (distance et Doppler calculés par le moteur).
