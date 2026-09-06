# Credits

Ressources tierces utilisées dans Artouste, avec leur licence d'origine.

## Données géographiques

- **Relief, orthophotos et emprises de bâtiments** : [IGN](https://www.ign.fr/)
  (Institut national de l'information géographique et forestière), jeux RGE ALTI,
  BD ORTHO, BD TOPO et LiDAR HD, sous
  [Licence Ouverte Etalab 2.0](https://www.etalab.gouv.fr/licence-ouverte-open-licence).
  Ces données servent à fabriquer les terrains, les orthophotos et les bâtiments
  3D des cartes livrées (`assets/terrain/`), ainsi que les tuiles fines
  téléchargées depuis le gestionnaire de cartes. Le LiDAR HD, par ses modèles
  dérivés MNT et MNS, sert en plus à fabriquer les monuments modelés d'après le
  terrain réel, à commencer par l'observatoire du Pic du Midi
  (`assets/models/monuments/pic-du-midi/`, voir `tools/observatoire.py`). La
  licence demande de citer la source : elle l'est ici, dans le README, sur la
  page de présentation et dans la notice PDF.

<img src="docs/IGN_logo_2012.png" alt="IGN" width="64" />

## Code tiers versionné

- **bc7enc** (<https://github.com/richgel999/bc7enc_rdo>) par Richard Geldreich,
  Jr., sous licence MIT ou domaine public au choix. Fichiers :
  `third_party/bc7enc/`, commit `dbe416d28a5530b4e8cc45b14bf034dc6b96bbde`.
  Compresseur de blocs BC7 : le moteur s'en sert pour précompresser
  l'orthophoto du terrain dans un cache local, au lieu de laisser le pilote
  graphique le faire à chaque lancement. L'auteur demande l'attribution sans
  l'exiger.

## Modèle 3D

- **Zombies animés (mode zombie)** : "Polyart Zombies with Animations Free Pack"
  (<https://sketchfab.com/3d-models/polyart-zombies-with-animations-free-pack-d9bcfdd88f5348549bc947226af7c314>)
  par Denys Almaral, sous licence Sketchfab Standard (usage libre avec
  attribution). Fichier : `assets/models/zombie/zombies_animated.glb`. C'est le
  modèle utilisé en jeu (personnages skinnés, marche et bras animés).

- **Explosion (mode zombie)** : "Timeframe Explosion"
  (<https://sketchfab.com/3d-models/timeframe-explosion-9e73437350dc4bcab9b2f3a4a044b16e>)
  par Jorma Rysky, sous licence
  [Creative Commons Attribution 4.0](http://creativecommons.org/licenses/by/4.0/).
  Fichier : `assets/models/zombie/explosion.glb`. Explosion 3D animée jouée à
  l'impact des roquettes.

- **Monuments de Paris** (de la tour Eiffel au Palais du Louvre) : scène
  FlightGear "Paris V2"
  (<http://embaranger.free.fr/flightgear/scenery/ParisV2/ParisV2.htm>) par
  Emmanuel Baranger (helijah), lui-même extrait des scènes X-Plane de Bertrand
  Augras (<http://baugras.club.fr/xplane/Site/france.html>). Fichiers :
  `assets/models/monuments/paris/`, un `.ac` par monument et trois atlas
  (`Texture01.png`, `Texture02.png`, `Texture03.png`) partagés entre eux. Les
  monuments arrivent un par un, chacun après sa vérification en vol, voir la
  liste à cocher de la ROADMAP.

  Sous licence **GPL v2**, comme le projet. L'archive d'origine ne l'annonçait
  nulle part et son `Read-Me.txt` se bornait à créditer l'auteur du travail
  X-Plane dont les modèles sont tirés ; Emmanuel Baranger a confirmé la licence
  par courriel le 28 juillet 2026.

## Sons

- **Départ de roquette (mode zombie)** : "Shotgun Fire"
  (<https://freesound.org/people/hyperix6/sounds/660299/>) par hyperix6, sous
  licence [Creative Commons 0](https://creativecommons.org/publicdomain/zero/1.0/)
  (domaine public). Fichier : `assets/sounds/combat/gunfire.wav`.

- **Explosion de roquette (mode zombie)** : "Large Explosion"
  (<https://freesound.org/people/TheSoundFXGuy_YT/sounds/534217/>) par
  TheSoundFXGuy_YT, sous licence
  [Creative Commons Attribution 4.0](http://creativecommons.org/licenses/by/4.0/).
  Fichier : `assets/sounds/combat/explosion.wav`.

- **Zombie touché (mode zombie)** : "female_growl2.wav"
  (<https://freesound.org/people/xpoki/sounds/432762/>) par xpoki, sous licence
  [Creative Commons Attribution 3.0](https://creativecommons.org/licenses/by/3.0/).
  Fichier : `assets/sounds/combat/zombie_hit.wav`.

- **Zombie tué avec fusée de bonus (mode zombie)** : "Zombie_36.wav"
  (<https://freesound.org/people/LittleRobotSoundFactory/sounds/316264/>) par
  LittleRobotSoundFactory, sous licence
  [Creative Commons Attribution 4.0](http://creativecommons.org/licenses/by/4.0/).
  Fichier : `assets/sounds/combat/zombie_death_bonus.wav`.

- **Zombie tué sans bonus (mode zombie)** : "Zombie Groan 0"
  (<https://freesound.org/people/OwNathan/sounds/754438/>) par OwNathan,
  sous licence [Creative Commons 0](https://creativecommons.org/publicdomain/zero/1.0/)
  (domaine public). Fichier : `assets/sounds/combat/zombie_death_simple.wav`.

- **Jet de pneu toxique (mode zombie)** : "Firework Launch (2)"
  (<https://freesound.org/people/LukaCafuka/sounds/750685/>) par LukaCafuka,
  sous licence [Creative Commons 0](https://creativecommons.org/publicdomain/zero/1.0/)
  (domaine public). Fichier : `assets/sounds/combat/toxic_throw.wav`.

- **Impact de pneu toxique (mode zombie)** : "Glass_Shards_Impact_04"
  (<https://freesound.org/people/BlondPanda/sounds/778607/>) par BlondPanda, sous
  licence [Creative Commons 0](https://creativecommons.org/publicdomain/zero/1.0/)
  (domaine public). Fichier : `assets/sounds/combat/toxic_impact.wav`.

- **Départ de la fusée de bonus (mode zombie)** : "explosion_high_to_low_1.wav"
  (<https://freesound.org/people/eardeer/sounds/402006/>) par eardeer, sous licence
  [Creative Commons 0](https://creativecommons.org/publicdomain/zero/1.0/)
  (domaine public). Fichier : `assets/sounds/combat/launch_sphere.wav`.

- **Apparition d'une sphère de bonus (mode zombie)** : "Firework Explosion 4"
  (<https://freesound.org/people/TB0Y298/sounds/719796/>) par TB0Y298, sous
  licence [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).
  Fichier : `assets/sounds/combat/sphere.wav`.

- **Apparition de la sphère de vie (mode zombie)** : "firework_explosion_fizz.wav"
  (<https://freesound.org/people/soundscalpel.com/sounds/110391/>) par
  soundscalpel.com, sous licence
  [Creative Commons Attribution 3.0](http://creativecommons.org/licenses/by/3.0/).
  Fichier : `assets/sounds/combat/sphere_sante.wav`.

- **Ramassage d'une sphère de bonus (mode zombie)** : "fizzy drink opening"
  (<https://freesound.org/people/JakesterTV/sounds/202317/>) par JakesterTV, sous
  licence [Creative Commons 0](https://creativecommons.org/publicdomain/zero/1.0/)
  (domaine public). Fichier : `assets/sounds/combat/drink.wav`.

- **Apparition du largueur (mode zombie)** : "Zombies.wav"
  (<https://freesound.org/people/xtrgamr/sounds/257790/>) par xtrgamr, sous
  licence [Creative Commons Attribution 4.0](http://creativecommons.org/licenses/by/4.0/).
  Fichier : `assets/sounds/combat/rale.wav`.

- **Nouvelle vague (mode zombie)** : "Mysterious Magical Bell Flourish"
  (<https://freesound.org/people/SkySpeira/sounds/848847/>) par SkySpeira, sous
  licence [Creative Commons 0](https://creativecommons.org/publicdomain/zero/1.0/)
  (domaine public). Fichier : `assets/sounds/combat/wave_start.wav`.
