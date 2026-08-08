## v0.31.0 - 8 août 2026

### Nouvelles fonctionnalités

- **Couleur des toitures lue dans l'orthophoto** : le zinc parisien, la tuile landaise et la tôle des hangars sortent tels quels, au lieu d'une palette de tuiles unique qui couvrait Paris de terre cuite.
- **Quatre monuments de Paris de plus** : Grand Palais, palais de Chaillot, palais du Luxembourg et École militaire.
- **Cap du marquage H des hélipads**, facultatif, dans `helipads.txt`. Le pad d'Hossegor regarde désormais l'ouest.
- **Brume réglable** par les clés `brume_debut` et `brume_fin` de `config.txt`.
- **Notice** : une page sur la mise à jour du jeu, et ce que deviennent les tuiles déjà téléchargées.

### Changements

- **Option `souffle` retirée** : le souffle rotor est toujours actif.

### Corrections

- **Curseur de la souris** visible pendant le chargement d'une carte.
- **Heure de départ** : la deuxième carte d'une session partait à 8h plus tout le temps déjà joué, soit en pleine nuit.
- **Hélipad de départ** dessiné deux fois quand la carte en déclarait un au même endroit.

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
