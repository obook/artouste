# Références

Sources documentaires du modèle de vol d'Artouste, les liens ont été vérifiés par téléchargement du document à la date indiquée.

Les sources sont classées par usage réel, et non par ordre d'importance : ce que
le code doit à un ouvrage est dit sous chaque entrée. Une source seulement
recommandée pour la suite est rangée à part, pour ne pas laisser croire qu'elle a
servi.

## Sources utilisées dans le code

Federal Aviation Administration. (2019). *Helicopter flying handbook*
(FAA-H-8083-21B). U.S. Department of Transportation.
https://www.faa.gov/regulations_policies/handbooks_manuals/aviation/helicopter_flying_handbook

> Chapitre 2, *Aerodynamics of flight*. Portance de translation, dissymétrie de
> portance, effet de flux transversal, vol latéral et arrière. C'est la source du
> comportement attendu au passage en translation : montée des vibrations vers
> 12 à 15 kt, soit 22 à 28 km/h au badin de l'Alouette II, qui est en unités
> métriques ; déplacement maximal 90 degrés plus loin dans le sens de rotation
> par précession gyroscopique, cabrage et roulis pendant la transition.
>
> ATTENTION à la transposition : le manuel décrit des rotors tournant en sens
> antihoraire vu de dessus (convention américaine) et annonce donc un roulis à
> DROITE. Le rotor de l'Alouette II tourne en sens horaire (convention
> française) : tous les sens latéraux sont à prendre en miroir, d'où le roulis à
> GAUCHE codé dans `TRANSVERSE_ROLL` et la pale reculante à droite de
> `RBS_ROLL` (voir `src/physics/constants.hpp`).
>
> Vérifié le 09/08/2026 : chapitre 2 téléchargé (12,7 Mo) depuis
> https://www.faa.gov/sites/faa.gov/files/regulations_policies/handbooks_manuals/aviation/helicopter_flying_handbook/hfh_ch02.pdf

## Sources consultées, en réserve pour une refonte

Ces rapports décrivent des modèles complets à plusieurs degrés de liberté, avec
battement pale par pale. Artouste s'en tient pour l'instant à un modèle de forces
additives, beaucoup plus simple ; ces documents sont donc vérifiés et archivés
comme référence, mais leurs équations ne sont pas implémentées.

Talbot, P. D., Tinling, B. E., Decker, W. A., & Chen, R. T. N. (1982). *A
mathematical model of a single main rotor helicopter for piloted simulation*
(NASA-TM-84281). National Aeronautics and Space Administration, Ames Research
Center. https://ntrs.nasa.gov/citations/19830001781

> Modèle à dix degrés de liberté (six de corps rigide, trois de battement, un de
> rotation rotor). C'est la structure d'équations la plus proche du besoin d'un
> simulateur piloté, si une refonte complète du moteur physique était engagée.
> Vérifié le 09/08/2026 : PDF de 52 pages, 1,7 Mo.

Weber, J. M., Liu, T. Y., & Chung, W. (1984). *A mathematical simulation model of
a CH-47B helicopter, volume 1* (NASA-TM-84351-VOL-1). National Aeronautics and
Space Administration, Ames Research Center.
https://ntrs.nasa.gov/citations/19850001726

> Exemple complet d'un modèle total-force à six degrés de liberté, avec les
> équations de battement de Wheatley-Bailey.
> Vérifié le 09/08/2026 : PDF de 136 pages, 4,4 Mo.

Aiken, E. W. (1980). *A mathematical representation of an advanced helicopter for
piloted simulator investigations of control system and display variations*
(NASA-TM-81203 / AVRADCOM-TM-80-A-02). National Aeronautics and Space
Administration, Ames Research Center. https://ntrs.nasa.gov/citations/19800019870

> Développement de Taylor autour d'une trajectoire de référence définie en
> fonction de la vitesse air longitudinale : utile pour linéariser localement la
> réponse du cyclique par plage de vitesse, plutôt que d'écrire un modèle global.
> Vérifié le 09/08/2026 : PDF de 95 pages, 1,8 Mo.

## Ouvrages recommandés, non consultés

Ouvrages sous droits, sans accès en ligne libre : ils sont cités pour mémoire,
aucune de leurs équations n'a servi. Références vérifiées auprès de Crossref et
d'Open Library, faute de pouvoir télécharger les textes.

Padfield, G. D. (2018). *Helicopter flight dynamics: Including a treatment of
tiltrotor aircraft* (3e éd.). Wiley. https://doi.org/10.1002/9781119401087

> Référence académique standard de la simulation pilotée : équations complètes de
> battement, écoulement dynamique et qualités de vol.

Bramwell, A. R. S., Done, G., & Balmford, D. (2001). *Bramwell's helicopter
dynamics* (2e éd.). Butterworth-Heinemann.

> Traitement historique et rigoureux des mêmes équations, souvent cité en base
> par Padfield.

Prouty, R. W. (1986). *Helicopter performance, stability, and control*. PWS
Engineering. (Réimpressions Krieger, 1990, 1995 et 2001)

> Plus orienté ingénierie pratique, largement utilisé pour la modélisation de
> simulateurs.

## Note sur l'appareil simulé

Artouste modélise un **SE 3130** à turbine Artouste II. Les performances chiffrées
les plus accessibles (185 km/h de vitesse maximale au niveau de la mer, 170 km/h
de croisière) sont publiées pour le **SE 313B**, variante ultérieure à turbine
Artouste IIC5 ou IIC6 et masse maximale portée de 1500 à 1600 kg. Même famille de
turbine et même puissance de 400 ch : à la masse simulée (1100 kg, chargement
léger), ces vitesses valent pour les deux.

Chiffres retenus du SE 313B, tous vérifiés le 09/08/2026 sur la fiche Wikipédia
et repris tels quels dans `src/physics/constants.hpp` : vitesse ascensionnelle
4,2 m/s, plafond pratique 2300 m, turbine Artouste IIC6 limitée à 269 kW en
utilisation opérationnelle.

Deux limites à ne pas confondre, et que le modèle distinguait mal avant le
09/08/2026 :

* **VNE 195 km/h** : limite STRUCTURELLE, propre au SE 3130. On ne l'atteint
  qu'en poussant sur le manche. C'est elle que porte la bande du cadran IAS
  (176-195 km/h) et que surveille le voyant de survitesse ;
* **185 km/h** : vitesse maximale EN PALIER, qui ne relève d'aucune limite mais
  de la puissance disponible face à la traînée. Elle doit sortir du calcul, pas
  d'une constante.

Confondre les deux faisait apparaître le décrochage de pale reculante dès la
croisière rapide, au lieu de la seule approche de la limite structurelle.

Sud Aviation. (s. d.). *Alouette II* [Fiche technique SE 3130 / SE 313B].
Wikipédia. https://en.wikipedia.org/wiki/Sud_Aviation_Alouette_II

> Vérifié le 09/08/2026 : sections *Specifications (SE 313B Alouette II)* et
> *Variants* relevées en ligne. Les procédures et limitations retenues pour le
> simulateur sont rassemblées dans `docs/PROCEDURE_VOL.md`.

## Données géographiques et modèle 3D

Ces sources ne relèvent pas du modèle de vol mais sont citées ici pour rassembler
en un seul endroit tout ce que le simulateur doit à des tiers.

Institut national de l'information géographique et forestière. (2026).
*Géoplateforme* [Jeux de données RGE ALTI, BD ORTHO, BD TOPO, BD Forêt V2, ADMIN
EXPRESS]. Licence Ouverte Etalab 2.0. https://data.geopf.fr

> Relief, orthophotos, bâtiments, masque de forêt et contours administratifs de
> toutes les cartes. Voir `docs/CARTES.md` et `tools/`.

FlightGear Flight Simulator. (2026). *FGData* [Modèle 3D d'Alouette II et
textures d'arbres]. GPL v2. https://gitlab.com/flightgear/fgdata

> Modèle de l'appareil et planches de l'atlas de végétation. Crédits détaillés
> dans `assets/models/Alouette-II/COPYING` et
> `assets/vegetation/fgdata-trees/CREDITS.txt`.
