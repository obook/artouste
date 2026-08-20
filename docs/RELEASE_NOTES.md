## v0.42.0 - 20 août 2026

### Nouvelles fonctionnalités

- **Aiguille-bille** : le HUD complet gagne son indicateur de virage, à gauche du variomètre. L'aiguille donne le taux de virage, avec ses repères du virage standard ; la bille dit si le virage est coordonné au palonnier.

### Changements

- **Girouette de dérive** : le fuselage se réaligne sur le vent relatif. L'appareil ne reste plus en crabe après un virage ou un coup de palonnier.

### Corrections

- **Atterrissage automatique sur un pad perché** : la pente d'approche se réfère désormais au niveau du pad. L'appareil passait sous le plateau, puis le contact le remettait dessus d'un coup.
- **Pad et repère du col du Tourmalet** : ils se trouvaient 115 m au sud du col et 33 m plus bas, sur le versant.

## v0.41.0 - 19 août 2026

### Nouvelles fonctionnalités

- **Sphères de bonus sur les kills** : une explosion qui fauche des zombies lance une fusée, qui monte poser une sphère à 50 m du sol. Kérosène pour un kill, vie pour un double, hécatombe pour un triple. Chaque coup de roquette coûte 2 L de carburant, ce qui donne un intérêt au ravitaillement.
- **Trois sons d'éclosion** : la sphère de kérosène, celle de vie et la tête de mort s'annoncent chacune avec le sien, joué là-haut où la sphère apparaît.
- **Sélection de carte en boucle** : dans le menu, après la dernière carte on revient à la première.

### Changements

- **Mode assisté et atterrissage automatique interdits en mode zombie.**
- **Pneus toxiques** : ce que lancent les zombies s'appelait des boulettes.

### Documentation

- **Chaîne du rendu des cartes**, du terrain nu au HR 3D : les quatre étapes, les fonctions en jeu, les formules, les difficultés rencontrées et les sources IGN.

## v0.40.0 - 18 août 2026

### Nouvelles fonctionnalités

- **Cartes améliorées depuis le jeu** : le gestionnaire de cartes fabrique désormais lui-même les tuiles d'orthophoto en haute résolution ET le relief 3D, récupérés auprès de l'IGN sans quitter le simulateur. La colonne Résolution annonce l'état de chaque carte (LR ou HR, 2D ou 3D, partiel, à refaire), et une fabrication interrompue reprend où elle s'est arrêtée.
- **Liserets de diagnostic du relief** : la clé `relief_debug` trace les frontières de la fenêtre de relief, contour du noyau en magenta, contour de l'anneau en cyan, cercles du fondu en orange et jaune, avec leur légende dans le HUD. Le terrain garde ses couleurs, le mode s'utilise en vol.
- **Bandeau VITESSE EXCESSIVE** : alerte rouge clignotante quand la VNE est franchie, du même style que TAUX DE DESCENTE.
- **Nouvelle page de notice, Des cartes plus vraies** : comment améliorer une carte, ce que cela coûte en disque et en temps.

### Corrections

- **Peigne de lames à la frontière des grilles de relief** : sur une arête vue en rasant, les triangles du noyau laissaient voir la corde de l'anneau, prise pour des pics qui se créaient à l'approche.
- **Fenêtre de relief bornée au bord de carte** : au-delà, elle répétait l'orthophoto et lisait le relief de l'autre côté de la carte.
- **Bornes géographiques en double précision** : en simple, l'arrondi décalait le relief de 80 cm sur les versants, et l'orthophoto d'autant.
- **Écran d'analyse animé** : le rouet tourne pendant les mesures, et non plus seulement entre elles.
- **Retour de fabrication** : seule la carte remaniée est remesurée, au lieu des onze.

## v0.33.0 - 12 août 2026

### Nouvelles fonctionnalités

- **Reflets, occlusion et relief sur le modèle** : reflets du soleil et du ciel, occlusion ambiante et carte de relief cuites avec Blender, posées sur la cabine et la planche de bord. La turbine garde la nuance de sa livrée en ressortant presque noire. Coût mesuré : 25,3 Mo de mémoire vidéo pour les textures du modèle, contre 10,7 avant.
- **Bâtiments habillés par paires de faces opposées** : les deux longues faces reçoivent la façade fenêtrée, les pignons un mur plein. Sans attribut de sommet en plus ni dessin supplémentaire.
- **Repères comblés sur toutes les cartes** : plus aucune zone importante sans nom. Chaque carte a été découpée en grille et les cases vides remplies avec des toponymes IGN et OpenStreetMap. Trente-six repères de plus, dont Cestas, Latresne et Artigues autour de Bordeaux, Luz-Ardiden et le pic de Cestrède sur Cauterets, Gourette et le pic de Ger sur Ossau, le parc de la Villette et le Parc des Princes sur Paris.
- **Hélipads recoupés avec les listes officielles** (BD TOPO de l'IGN, hélistations hospitalières de data.gouv.fr) : quatre hélistations manquaient, à Arès, Villenave-d'Ornon, Mérignac-Beutre et Pau-Pyrénées.
- **Saubrigues** ajoutée à la côte landaise.

### Changements

- **Flux transversal adouci**, de 900 à 450 N.m. Le roulis à gauche au passage en translation restait à contrer d'une pointe de cyclique, mais il arrivait comme une embardée que le rappel d'assiette rattrapait ensuite tout seul.
- **Refroidissement de la tuyère** : la TMP revenait à l'ambiante en une demi-minute. Elle décroche maintenant vers 250 degrés en quelques secondes, puis suit la chaleur du métal : environ 95 degrés cinq minutes après la coupure, 40 au bout de dix.

### Corrections

- **Écran des cartes** : le seuil HR/LR n'était pas celui du moteur, un jeu de tuiles entre les deux s'affichait LR alors qu'il était bien chargé. Les deux passent maintenant par la même règle.
- **Jeu de tuiles interrompu** : un niveau serré inachevé laissait la carte annoncée entière, et une fabrication lancée en ligne de commande ne posait aucun témoin. Les deux chemins posent désormais le même.
- **Golf de Saint-Paul-lès-Dax** retiré : il n'y a pas de parcours à cet endroit, l'orthophoto montre de la forêt.
- **Pitié-Salpêtrière** et **Bordeaux-Mérignac** recalés sur leur aire de poser officielle, à 290 et 233 m de leur ancien point.
- **Magescq** retirée de la côte landaise : la commune est à 6,6 km à l'est du bord de la carte.

### Documentation

- `docs/HELIPADS.md` réécrit : les deux listes officielles, leurs pièges (une erreur de coordonnées connue dans la liste hospitalière) et le nettoyage à faire avant d'ajouter un point.

## v0.32.0 - 9 août 2026

### Nouvelles fonctionnalités

- **Regard du pilote en vue cockpit** (manette) : L3 maintenu, puis stick droit pour tourner la tête. Relâcher ramène la vue vers l'avant.
- **Radio internet à la manette** : croix directionnelle droite pour allumer ou couper, haut/bas pour la balance radio/hélico.
- **Collectif gradué en degrés de pas**, comme la machine : 6 à 15 degrés, stationnaire à 11, butée élastique à 14,5 signalée en jaune, secours à 15. Le cadran et le coin du HUD affichent le pas réel.
- **Régime de décollage** : en altitude la turbine tient un plancher de puissance, ce qui porte le plafond de stationnaire hors effet de sol à 4078 m au lieu de 3090. La tuyère chauffe d'autant, et l'alarme TMP dit quand il est temps de redescendre.
- **Indicateur de zone hauteur-vitesse**, optionnel : clé `zone_hv` de `config.txt`, éteint par défaut.

### Changements

- **Enveloppe de vol refaite.** La montée, la vitesse maximale et l'assiette de croisière sortent maintenant d'un bilan de puissance, au lieu d'être trois réglages séparés qui se contredisaient. L'appareil pique de 8 degrés en croisière au lieu de 20, plafonne à 188 km/h en palier, et grimpe à 9,8 m/s au niveau de la mer.
- **Montée en montagne** : le vario était bloqué vers 1 m/s sur la carte ossau, qui en devenait injouable. Il monte à 7,9 m/s au décollage, et le plafond passe de 2300 à 3300 m.
- **VNE en altitude** moins sévère : la limite reste pleine jusqu'à 1829 m, comme au placard de l'appareil, au lieu de baisser dès le niveau de la mer.
- **Approche automatique** un peu plus douce : la décélération commence plus tôt, la cellule freinant moins qu'avant.

### Corrections

- **Radio internet** : léger gel à l'extinction, le temps que le thread réseau se termine.
- **Mode zombie optionnel** retiré du menu (touche Z) : sans effet, faute de carte compatible non dédiée.
- **Arbres sur les aires de poser** : le dégagement était centré à côté du pad, et les pads autres que celui de départ n'en avaient aucun. Aucun arbre à moins de 50 m d'un pad, sur toutes les cartes.

### Documentation

- Sources chiffrées de l'Alouette II (`REFERENCES.md`), et données de référence dans `docs/technique/` : chaque valeur porte sa source et ses conditions, les sources qui divergent sont conservées toutes les deux.
- **Planches du manuel de vol allemand** reconstituées avec leurs figures (`docs/technique/planches-manuel-vol.pdf`) : montée, carte moteur, puissances de décollage, butées de pas, VNE, domaine d'autorotation, masses maximales avec et sans effet de sol.

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
