# PANEL.md - Tableau de bord SE 3130 Alouette II

Relevé complet des instruments et commandes du tableau de bord de l'Alouette II SE 3130,
d'après le Flight Manual officiel Sud Aviation (SGAC approved, figures 2-5 et 2-5a,
appendice 1 figure 1-1). Deux variantes documentées : appareils n° 1327 et inférieurs
(figure 2-5), et appareils n° 1328 et supérieurs (figure 2-5a, légende commune).

> Ce fichier sert de référence pour l'implémentation du HUD et du cockpit 3D du simulateur
> Artouste. Ne pas utiliser comme référence de vol réel.

---

## 1. Instruments de vol et de navigation

### 1.1 Tachymètre double (item 8 - "Engine-rotor tachometer" / Tachymètre moteur-rotor)

Instrument central du tableau de bord. Cadran unique à deux aiguilles concentriques.

**Aiguille ROTOR (intérieure) :**

- Graduation : 0 à 40 (x 10 tr/min), soit 0 à 420 tr/min effectifs.
- Zone verte : 340 à 380 tr/min (plage nominale de vol).
- Régime nominal : 360 tr/min.
- Limite basse d'alerte : < 320 tr/min (risque de décrochage rotor).
- Limite haute absolue : 420 tr/min.

**Aiguille ENGINE / MOTEUR (extérieure) :**

- Graduation : 0 à 40 (x 1 000 tr/min), soit 0 à 40 000 tr/min effectifs.
- Régime ralenti (turbine seule, frein rotor serré) : 19 000 tr/min.
- Régime nominal (rotor engagé) : 33 000 à 34 000 tr/min.
- Limite absolue (ligne rouge) : 35 000 tr/min. Ne jamais dépasser.

**Implémentation simulateur :**

```
tachymetre_rotor   : float  # 0 a 420 tr/min
tachymetre_turbine : float  # 0 a 35 000 tr/min
zone_verte_rotor   : (340, 380)
zone_rouge_turbine : (35000, ...)
```

---

### 1.2 Indicateur de vitesse air (item 40 - "Airspeed indicator" / Indicateur de vitesse)

- Graduation : 20 à 140 kt.
- Repère minimum : 30 kt (début de plage utilisable).
- Arc vert : 95 à 105 kt (plage de croisière économique).
- Arc jaune : 105 à 120 kt (utilisation avec précaution).
- VNE (Velocity Never Exceed / Vitesse à ne jamais dépasser) : 120 kt (ligne rouge).
- Correspondances indicatives : 30 kt = 55 km/h, 95 kt = 176 km/h, 120 kt = 222 km/h.

**Implémentation simulateur :**

```
vitesse_air_kt : float  # 0 a 140 kt
vne_kt         : 120
arc_vert       : (95, 105)
```

---

### 1.3 Altimètre (item 41 - "Altimeter" / Altimètre)

- Type : altimètre barométrique standard.
- Graduation : en pieds (ft), deux aiguilles (centaines et milliers).
- Plafond opérationnel SE 3130 : 3 200 m (en charge) / 4 700 m (à vide).
- Pas de valeur limite inscrite sur le cadran.

**Implémentation simulateur :** affichage en mètres ou en pieds selon préférence utilisateur.

---

### 1.4 Variomètre (item 7 - "Rate-of-climb indicator" / Indicateur de vitesse verticale)

- Graduation : montée et descente, en pieds/minute ou en m/s selon version.
- Taux de montée nominal à puissance maxi : environ 4,8 m/s (945 ft/min).
- Pas de zone rouge spécifique : l'instrument est purement informatif.

---

### 1.5 Compas magnétique (item 3 - "Magnetic compass" / Compas magnétique)

- Situé en haut du tableau, au centre.
- Compas de route à lecture directe, graduation 0 à 360 degrés.
- Sensible aux accélérations : lecture fiable uniquement en vol stabilisé.

---

### 1.6 Indicateur de pas collectif (item 42 - "Collective-pitch indicator" / Indicateur de pas collectif)

- Cadran semi-circulaire, graduation 0 à 14,5° ou 15° selon version.
- Reflet direct de l'angle de calage des pales du rotor principal.
- Correspondance avec le simulateur : 0 % collectif = 0°, 100 % = 14,5°.
- Utilisation typique au décollage : 6 à 8° (environ 40 à 55 %).

---

## 2. Instruments moteur

### 2.1 Unité triple (item 10 - "Three-fold gauge unit" / Instrument triple)

Cadran unique regroupant trois mesures indépendantes. Situé en haut à droite du
panneau principal.

**Mesure 1 : Température tuyère d'échappement ("tail pipe" / tuyère)**

- Graduation : 0 à 550°C.
- Température normale en vol : 400 à 480°C.
- Limite maximale : 500°C en continu, 550°C transitoire.
- Dépassement : extinction moteur possible, risque de dommages turbine.

**Mesure 2 : Température huile moteur ("engine oil temperature" / température huile)**

- Graduation : en °C x 10.
- Note du manuel : la plage verte dépend du type d'huile utilisé.
  - Huile AIR 3515A : limite 0 à 35°C (arc vert).
  - Huile DED 2480 : limite 0 à 55°C (arc vert).
- Hors plage verte : réduire la puissance et surveiller.

**Mesure 3 : Pression huile moteur ("engine oil pressure" / pression huile)**

- Graduation : 0 à 0,8 bar.
- Pression normale : 0,5 à 0,7 bar en vol.
- Limite basse : < 0,3 bar - alerte immédiate, arrêter le moteur.

**Implémentation simulateur :**

```
temperature_tuyere  : float  # 0 a 550 degres C
temperature_huile   : float  # degres C
pression_huile      : float  # 0 a 0,8 bar
seuil_alerte_huile  : 0.3    # bar
seuil_alerte_tuyere : 500    # degres C
```

---

### 2.2 Indicateur de carburant (item 47 - "Fuel contents gauge" / Jauge de carburant)

- Graduation : 0 à 15 (US gallons x 10), soit 0 à 150 gallons = 580 litres environ.
- Voyant d'alerte bas carburant : allumage à 4 US gallons (environ 15 litres).
- Consommation indicative : 110 kg/h à vitesse économique, 155 kg/h à puissance maxi.
- Autonomie approximative : 4 h à vitesse économique.

---

### 2.3 Voyant pression huile transmission (item 6 - "Transmission oil pressure warning light" / Voyant pression huile transmission)

- Voyant rouge uniquement, pas de cadran analogique séparé.
- S'allume en cas de chute de pression huile de la boîte de transmission principale.
- Alerte critique : atterrissage immédiat requis.

---

### 2.4 Jauge de pression servo-commandes (item 13 - "Servo unit pressure gauge" / Manomètre servo-commandes)

- Optionnel, présent sur les appareils équipés de servo-commandes (à partir du n° 285).
- Graduation non précisée dans les images disponibles.
- Absence de pression : commandes en effort direct (cas du SE 3130 de base sans servo).

---

## 3. Voyants d'alerte et signalisation

Tous les voyants suivants sont référencés dans la légende officielle (image 2).

| Item | Désignation officielle (anglais) | Traduction française | Couleur | Condition d'allumage |
|---|---|---|---|---|
| 5 | Red light | Voyant rouge général | Rouge | Anomalie critique non spécifique |
| 6 | Transmission oil pressure warning light | Voyant pression huile transmission | Rouge | Chute pression huile transmission |
| 9 | Fuel pressure warning light | Voyant pression carburant | Rouge | Chute pression carburant |
| 17 | Starter relay warning light | Voyant relais démarreur | Rouge | Anomalie relais démarreur |
| 19 | Starter warning light | Voyant démarreur en marche | Vert | Démarreur en cours d'utilisation |
| 20 | Micropump warning light | Voyant micropompe | Orange | Pompe micropression en défaut |
| 36 | Fuel filter clogging warning light | Voyant filtre carburant colmaté | Variable | Filtre carburant obstrué |
| 39 | Maximum fuel flow warning light | Voyant débit carburant maxi | Rouge | Débit carburant maximum atteint |
| 48 | Generator warning light | Voyant générateur | Rouge | Générateur hors service |
| 66 | Pitot head heating indicator light | Voyant chauffage sonde Pitot | Variable | Optionnel |

---

## 4. Commandes de puissance et carburant (partie basse du tableau)

### 4.1 Levier de débit carburant (item 68 - "Fuel flow control lever" / Levier de débit carburant)

- Commande directe du débit d'alimentation de la chambre de combustion.
- Position arrière = débit coupé (extinction).
- Position avant = plein débit.
- Utilisé en complément du régulateur automatique.

### 4.2 Levier du régulateur (item 69 - "Governor control lever" / Levier du régulateur)

- Régule automatiquement le régime turbine pour maintenir le régime rotor constant.
- Position nominale : le régulateur maintient 33 000 à 34 000 tr/min turbine.
- La vitesse de rotation de l'ensemble turbine-rotor est maintenue constante par
  le régulateur de vitesse du turbomoteur, quelle que soit la charge collective.

### 4.3 Levier de coupure carburant (item 70 - "Fuel shut-off cock control lever" / Robinet de coupure carburant)

- Coupure totale de l'alimentation carburant.
- Position fermée : extinction turbine (utilisée en fin de procédure d'arrêt).
- Distinct du robinet électrique carburant (commande séparée, item 2 de la procédure).

### 4.4 Robinet électrique carburant (consigne DGAC 1996)

- Non représenté comme levier physique visible mais essentiel en procédure.
- Interrupteur électrique commandant l'ouverture/fermeture du robinet principal.
- Vérification obligatoire du micro-switch à chaque mise en route (CN F-1996-072-053).

---

## 5. Commutateurs et disjoncteurs

### 5.1 Commutation électrique principale

| Item | Désignation officielle (anglais) | Traduction française | Type |
|---|---|---|---|
| 15 | Voltmeter | Voltmètre | Instrument |
| 16 | Booster pump switch | Interrupteur pompe de gavage | Interrupteur |
| 18 | Engine selector switch | Sélecteur moteur | Sélecteur |
| 22 | 24-volt receptacle circuit breaker | Disjoncteur prise 24 V | Disjoncteur |
| 23 | UV lighting circuit breaker | Disjoncteur éclairage UV | Disjoncteur |
| 24 | Emergency lighting circuit breaker | Disjoncteur éclairage de secours | Disjoncteur (optionnel) |
| 25 | Landing light circuit breaker | Disjoncteur phare d'atterrissage | Disjoncteur (optionnel) |
| 50 | Generator ON-OFF switch | Interrupteur générateur | Interrupteur |
| 51 | Battery master switch | Interrupteur général batterie | Interrupteur |
| 55 | Instrument panel circuit breaker | Disjoncteur tableau de bord | Disjoncteur |
| 56 | Starter circuit breaker | Disjoncteur démarreur | Disjoncteur |

### 5.2 Commutation radio et communications

| Item | Désignation officielle (anglais) | Traduction française | Type |
|---|---|---|---|
| 31 | VHF radio set | Poste radio VHF | Radio |
| 53 | Radio circuit breaker | Disjoncteur radio | Disjoncteur |
| 60 | Radio master switch | Interrupteur général radio | Interrupteur |
| 61 | Interphone master switch | Interrupteur général interphone | Interrupteur |
| 63 | Interphone selector switches | Sélecteurs interphone | Sélecteur |
| 57 | Interphone rheostat | Rhéostat interphone | Rhéostat |

### 5.3 Commutation éclairage

| Item | Désignation officielle (anglais) | Traduction française |
|---|---|---|
| 26 | UV lighting rheostat | Rhéostat éclairage UV |
| 27 | Cabin light circuit breaker | Disjoncteur éclairage cabine |
| 28 | Emergency lighting rheostat | Rhéostat éclairage de secours |
| 58 | Cabin lighting rheostat | Rhéostat lumière cabine |
| 29 | Position lights switch | Interrupteur feux de position |
| 30 | Anti-collision lights switch | Interrupteur feux anti-collision (optionnel) |

### 5.4 Commutation équipements optionnels

| Item | Désignation officielle (anglais) | Traduction française |
|---|---|---|
| 33 | Emergency flotation gear switch | Interrupteur train de flottaison de secours |
| 34 | Heating blankets switch | Interrupteur couvertures chauffantes |
| 35 | Servo unit 2-position control cock | Robinet de commande servo 2 positions (optionnel) |
| 59 | Spray equipment or hoist master switch | Interrupteur général pulvérisateur ou treuil |
| 62 | Fuel jettison master switch | Interrupteur largage carburant |
| 64 | Lateral trim cock | Robinet de trim latéral |

---

## 6. Réceptacles et prises

| Item | Désignation officielle (anglais) | Traduction française |
|---|---|---|
| 14 | 24-volt receptacle | Prise 24 volts |
| 32 | 24-volt receptacle switch (de-froster, etc.) | Interrupteur prise 24 V (dégivrage, etc.) |
| 43 | Flotation gear receptacle | Prise train de flottaison |

---

## 7. Commandes de vol (pédalier bas)

| Item | Désignation officielle (anglais) | Traduction française | Remarque |
|---|---|---|---|
| 67 | Pitot head heating control switch | Interrupteur chauffage sonde Pitot | Optionnel |
| 68 | Fuel flow control lever | Levier de débit carburant | Voir section 4.1 |
| 69 | Governor control lever | Levier du régulateur | Voir section 4.2 |
| 70 | Fuel shut-off cock control lever | Robinet de coupure carburant | Voir section 4.3 |
| 71 | Adjustable stop | Butée réglable du levier | - |

---

## 8. Affichage et visière

| Item | Désignation officielle (anglais) | Traduction française |
|---|---|---|
| 1 | Airspeed limitation placard | Plaquette de limitation de vitesse |
| 2 | Compass visor | Visière de compas |
| 4 | Instrument panel visor and lighting | Visière et éclairage tableau de bord |
| 11 | Oil temp. placard | Plaquette température huile |
| 37 | Outside air temp. indicator | Indicateur de température extérieure |
| 38 | UV light | Lampe UV |
| 65 | UV light | Lampe UV |
| 49 | Flap for engine power check diagram | Volet du diagramme de contrôle puissance moteur |

---

## ANNEXE - Instruments et commandes nécessaires au simulateur Artouste

Cette annexe définit strictement ce qui doit être implémenté dans le HUD et le
système de commandes du simulateur pour un vol complet et réaliste.
Tout le reste (radio, éclairage, optionnels) est hors périmètre de la version de base.

---

### A1. HUD - Instruments affichables en temps réel

#### Priorité 1 : indispensables au vol

| Instrument | Variable interne | Plage | Zones colorées |
|---|---|---|---|
| Tachymètre rotor | `tachymetre_rotor` | 0 à 420 tr/min | Vert : 340-380 / Rouge : > 400 |
| Tachymètre turbine | `tachymetre_turbine` | 0 à 35 000 tr/min | Vert : 33 000-34 000 / Rouge : > 35 000 |
| Vitesse air | `vitesse_air_kt` | 0 à 140 kt | Vert : 95-105 / Jaune : 105-120 / Rouge : > 120 |
| Altimètre | `altitude_m` | 0 à 5 000 m | Aucune |
| Variomètre | `taux_montee_ms` | -15 à +15 m/s | Aucune |
| Collectif | `collectif` | 0 à 100 % | Aucune |
| Cap | `cap_degres` | 0 à 360° | Aucune |

#### Priorité 2 : sécurité moteur

| Instrument | Variable interne | Plage | Seuil d'alerte |
|---|---|---|---|
| Température tuyère | `temperature_tuyere` | 0 à 550°C | Voyant rouge > 500°C |
| Pression huile moteur | `pression_huile` | 0 à 0,8 bar | Voyant rouge < 0,3 bar |
| Carburant | `carburant_litres` | 0 à 580 L | Voyant orange < 15 L |

#### Priorité 3 : information complémentaire

| Instrument | Variable interne | Remarque |
|---|---|---|
| Pas collectif en degrés | `pas_collectif_degres` | Dérivé de `collectif` x 14,5 |
| Phase de vol | `phase_vol` | Texte : ARRET, DEMARRAGE, EN VOL, etc. |
| Frein rotor | `frein_rotor` | Indicateur booléen (SERRE / LIBRE) |

---

### A2. Voyants d'alerte à implémenter

| Voyant | Variable déclenchante | Couleur | Condition |
|---|---|---|---|
| ROTOR BAS | `tachymetre_rotor` | Rouge | < 320 tr/min en vol |
| ROTOR HAUT | `tachymetre_rotor` | Rouge | > 400 tr/min |
| TURBINE LIMITE | `tachymetre_turbine` | Rouge | > 34 000 tr/min |
| TURBINE MAXI | `tachymetre_turbine` | Rouge clignotant | > 35 000 tr/min |
| PRESSION HUILE | `pression_huile` | Rouge | < 0,3 bar |
| TEMPERATURE | `temperature_tuyere` | Orange | > 480°C |
| TEMPERATURE MAXI | `temperature_tuyere` | Rouge | > 500°C |
| CARBURANT BAS | `carburant_litres` | Orange | < 15 L |
| CARBURANT VIDE | `carburant_litres` | Rouge | < 4 L |

---

### A3. Commandes de vol à implémenter (commandes réelles du simulateur)

Source : tableau des commandes fourni par le développeur.

#### Axes de vol

| Commande | Clavier AZERTY | Manette Xbox | Remarque |
|---|---|---|---|
| Collectif + | W ou Z | Gâchette RT | Incrémental clavier, analogique manette |
| Collectif - | S | Gâchette LT | Incrémental clavier, analogique manette |
| Cyclique (4 axes) | Flèches directionnelles | Stick gauche | Binaire clavier, analogique manette |
| Palonnier gauche | A ou Q | Stick droit X (gauche) | - |
| Palonnier droit | D | Stick droit X (droite) | - |

#### Commandes de séquence et interface

| Commande | Clavier AZERTY | Manette Xbox | Remarque |
|---|---|---|---|
| Turbine démarrer / couper | T | Bouton Start | Déclenche la séquence complète |
| Vue (cycle poursuite / cockpit / orbite) | C | Bouton Y (jaune) | Cycle entre les modes caméra |
| HUD on / off | H | Bouton B | - |
| Pause | P | Bouton Back | - |
| Réinitialiser la position | R | Bouton X | - |
| Quitter | Échap | LB + RB simultanés | Combinaison pour éviter le déclenchement accidentel |

---

### A4. Logique d'affichage HUD recommandée

```
Disposition suggérée (vue cockpit, coins d'écran) :

  [CAP 270]                              [ALT 1250 m]
  [IAS  85 kt]                           [VSI +2,5 m/s]

  [ROTOR   360 tr/min  |||||||||| ]
  [TURBINE 33500 tr/min ======== ]
  [COLLECTIF 55 %       ======   ]

  [TMP 445 C]  [HUILE 0,6 bar]  [FUEL 280 L]

  [PHASE : EN VOL]               [FREIN : LIBRE]

  [!] VOYANTS : zone d'alerte en bas au centre, fond rouge si critique
```

#### Règles d'affichage

- En phase ARRET ou DEMARRAGE : afficher prioritairement les paramètres moteur
  (tachymètre turbine, tachymètre rotor, frein rotor, phase).
- En vol : masquer frein rotor et phase, afficher tous les instruments de vol.
- Voyants d'alerte : toujours visibles en surimpression, quelle que soit la phase.
- Collectif : barre de progression horizontale en % suffit, pas de cadran circulaire.
- Tachymètres : cadrans analogiques ou barres de progression avec zones colorées.

---

### A5. Valeurs de référence pour la calibration

| Paramètre | Valeur de référence | Usage |
|---|---|---|
| Décollage (collectif minimum) | 40 à 55 % / 6 à 8° | En conditions standard, masse nominale |
| Vitesse économique | 85 à 90 kt | Consommation minimale |
| Vitesse de croisière normale | 95 à 105 kt | Zone verte IAS |
| VNE | 120 kt | Ne pas dépasser |
| Régime rotor nominal | 360 tr/min | Maintenu par le régulateur |
| Régime turbine nominal | 33 000 à 34 000 tr/min | Maintenu par le régulateur |
| Température tuyère croisière | 420 à 460°C | Normale |
| Consommation croisière | 110 kg/h | À 90 kt, masse nominale |
| Autonomie | environ 4 h | Réservoir plein, vitesse économique |
