## v0.31.0 - 8 août 2026

### Nouvelles fonctionnalités

- **Couleur des toitures lue dans l'orthophoto** : chaque bâtiment prend la teinte du toit photographié sous lui, au lieu d'une palette de tuiles codée en dur qui couvrait Paris de terre cuite. Le zinc parisien, la tuile landaise et les toits plats des zones d'activité sortent maintenant tels quels, sans rien déclarer carte par carte. La lecture n'a lieu que si l'emprise du bâtiment couvre au moins deux pixels d'orthophoto, les cartes allant de 0,85 m par pixel (Dax) à 9,8 (Arcachon) ; en dessous, un pixel mélange la maison, le jardin et les arbres, et la palette d'origine reprend la main. Les valeurs sont étalées d'autant plus que la photo est grise, une orthophoto de ville étant souvent presque achromatique.
- **Quatre monuments de Paris de plus** : Grand Palais (avec le Petit Palais et le pont Alexandre III), palais de Chaillot, palais du Luxembourg et École militaire, chacun calé sur l'orthophoto puis vérifié.
- **Cap du marquage H des hélipads** : `helipads.txt` accepte un cap facultatif avant le nom, qui oriente l'axe des montants du H. Le pad d'Hossegor regarde désormais l'ouest ; les autres, sans cap, gardent leur orientation nord-sud.
- **Page "Mettre à jour le jeu" dans la notice** : que faire quand on a déjà téléchargé des tuiles de détail et qu'une nouvelle version sort. En résumé, extraire par-dessus l'ancien dossier ; ni les tuiles ni la configuration ne voyagent dans l'archive.

### Corrections

- **Heure de départ des cartes suivantes** : l'heure était bien remise à 8h au chargement, mais comptée depuis le lancement du programme. La deuxième carte d'une session reprenait donc 8h plus tout le temps déjà joué, soit la nuit noire après un quart d'heure de vol. Chaque carte commence maintenant à son heure de départ.
- **Hélipad de départ dessiné deux fois** : le pad du point de départ et celui que la carte déclare au même endroit se superposaient. Sans conséquence tant qu'ils étaient identiques, mais deux H de caps différents se seraient croisés.

## v0.30.3 - 8 août 2026

### Nouvelles fonctionnalités

- **Trois monuments de Paris de plus** : Tour Saint-Jacques, Sainte-Chapelle et palais du Louvre, chacun posé après vérification visuelle. Position du Louvre approximative, comme les Invalides ou le Front de Seine.
- **Tour de contrôle de Dax-Seyresse** : nouveau lieu remarquable, signalé par un pilote.

### Corrections

- **Panneau combat du mode zombie** : recouvrait le ruban d'altitude et le cadran V/S en Super HUD depuis leur passage à droite (v0.30.2). Déplacé en haut à droite, seul coin encore libre.
- **Fond du premier écran de chargement** : ne se chargeait jamais, un chemin relatif utilisé avant que le dossier des ressources soit connu.

## v0.30.2 - 2 août 2026

### Nouvelles fonctionnalités

- **Nouvelle disposition du Super HUD** : collectif et IAS en bas à gauche, altitude et vario à droite (aiguille du vario repartant de l'horizontale comme un vrai VSI, avec un repère en pointillés à la valeur médiane), groupe central réduit à la turbine, au NR, à la température et au carburant. Disposition demandée par un pilote réel.

### Corrections

- **Virage coordonné** : avec de la vitesse, incliner au cyclique latéral tourne maintenant le nez dans le même sens, comme un avion. Au stationnaire, le cyclique latéral incline seulement l'appareil, qui part en crabe, pratique pour se décaler au posé. Retour d'un pilote réel.
- **Gamepad Freebox et autres manettes DragonRise PC TWIN SHOCK** : elles n'étaient pas reconnues du tout. Les correspondances existantes les décrivaient avec cinq axes alors qu'elles n'en exposent que quatre, ce qui suffisait à les faire rejeter en bloc et à laisser le simulateur sans aucune commande. Un fichier de correspondances propre au projet, chargé après la base communautaire et prioritaire sur elle, corrige le tir sans toucher à cette dernière. Attention, ces manettes exigent le mode analogique : diode éteinte, le stick droit ne pilote pas le palonnier mais recopie les quatre boutons de face.
- **Outil `gamepad_probe`** : il affiche désormais le GUID SDL de chaque manette, son état brut en permanence, et le relevé des amplitudes parcourues par chaque axe depuis le lancement. De quoi distinguer un axe mort d'un axe centré, et écrire la ligne de correspondance d'une manette inconnue.

## v0.30.1 - 2 août 2026

### Nouvelles fonctionnalités

- **Quatre monuments de Paris de plus** : Église de la Madeleine, tour Montparnasse, Grande Arche de la Défense et Hôtel de Ville, chacun posé après vérification en vol.

### Corrections

- **Palonnier plus vif** : le couple de lacet à pleine commande était retombé trop bas et rendait le virage sur place poussif ; l'appareil tourne de nouveau sur lui-même à environ 50 degrés par seconde.
- **Curseur de la souris** : il restait affiché au lancement tant qu'on ne bougeait pas la souris. Il est désormais masqué à l'entrée en vol et rendu au retour au menu, où les clics en ont besoin.
- **Grande Arche de la Défense** : orientation corrigée d'un quart de tour, son emprise carrée ne disant rien du sens du passage, et échelle revue, le modèle étant écrasé de 6 % en profondeur par rapport au monument.
