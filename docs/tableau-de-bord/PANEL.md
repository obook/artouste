# PANEL.md - Tableau de bord SE 3130 Alouette II

Relevé complet des instruments et commandes du tableau de bord de l'Alouette II SE 3130,
d'après le Flight Manual officiel Sud Aviation (SGAC approved, figures 2-5 et 2-5a,
appendice 1 figure 1-1). Deux variantes documentees : appareils n° 1327 et inferieurs
(figure 2-5), et appareils n° 1328 et superieurs (figure 2-5a, legende commune).

> Ce fichier sert de reference pour l'implementation du HUD et du cockpit 3D du simulateur
> Artouste. Ne pas utiliser comme reference de vol reel.

---

## 1. Instruments de vol et de navigation

### 1.1 Tachymetre double (item 8 - "Engine-rotor tachometer")

Instrument central du tableau de bord. Cadran unique a deux aiguilles concentriques.

**Aiguille ROTOR (interieure) :**

- Graduation : 0 a 40 (x 10 tr/min), soit 0 a 420 tr/min effectifs.
- Zone verte : 340 a 380 tr/min (plage nominale de vol).
- Regime nominal : 360 tr/min.
- Limite basse d'alerte : < 320 tr/min (risque de decrochage rotor).
- Limite haute absolue : 420 tr/min.

**Aiguille ENGINE (exterieure) :**

- Graduation : 0 a 40 (x 1 000 tr/min), soit 0 a 40 000 tr/min effectifs.
- Regime ralenti (turbine seule, frein rotor serre) : 19 000 tr/min.
- Regime nominal (rotor engage) : 33 000 a 34 000 tr/min.
- Limite absolue (ligne rouge) : 35 000 tr/min. Ne jamais depasser.

**Implementation simulateur :**

```
tachymetre_rotor   : float  # 0 a 420 tr/min
tachymetre_turbine : float  # 0 a 35 000 tr/min
zone_verte_rotor   : (340, 380)
zone_rouge_turbine : (35 000, ...)
```

---

### 1.2 Indicateur de vitesse air (item 40 - "Airspeed indicator")

- Graduation : 20 a 140 kt.
- Repere minimum : 30 kt (debut de plage utilisable).
- Arc vert : 95 a 105 kt (plage de croisiere economique).
- Arc jaune : 105 a 120 kt (utilisation avec precaution).
- VNE (Velocity Never Exceed) : 120 kt (ligne rouge).
- Correspondances indicatives : 30 kt = 55 km/h, 95 kt = 176 km/h, 120 kt = 222 km/h.

**Implementation simulateur :**

```
vitesse_air_kt : float  # 0 a 140 kt
vne_kt         : 120
arc_vert       : (95, 105)
```

---

### 1.3 Altimetre (item 41 - "Altimeter")

- Type : altimetre barometrique standard.
- Graduation : en pieds (ft), deux aiguilles (centaines et milliers).
- Plafond operationnel SE 3130 : 3 200 m (charge) / 4 700 m (vide).
- Pas de valeur limite inscrite sur le cadran.

**Implementation simulateur :** affichage en metres ou pieds selon preference utilisateur.

---

### 1.4 Variometre (item 7 - "Rate-of-climb indicator")

- Graduation : montee et descente, en pieds/minute ou m/s selon version.
- Taux de montee nominal a puissance maxi : environ 4,8 m/s (945 ft/min).
- Pas de zone rouge specifique : l'instrument est informatif.

---

### 1.5 Compas magnetique (item 3 - "Magnetic compass")

- Situe en haut du tableau, au centre.
- Compas de route a lecture directe, graduation 0 a 360 degres.
- Sensible aux accelerations : lecture fiable uniquement en vol stabilise.

---

### 1.6 Indicateur de pas collectif (item 42 - "Collective-pitch indicator")

- Cadran semi-circulaire, graduation 0 a 14,5° ou 15° selon version.
- Reflet direct de l'angle de calage des pales du rotor principal.
- Correspondance avec le simulateur : 0 % collectif = 0°, 100 % = 14,5°.
- Utilisation typique au decollage : 6 a 8° (environ 40 a 55 %).

---

## 2. Instruments moteur

### 2.1 Unite triple (item 10 - "Three-fold gauge unit")

Cadran unique regroupant trois mesures independantes. Situe en haut a droite du
panneau principal.

**Mesure 1 : Temperature tuyere d'echappement (tail pipe)**

- Graduation : 0 a 550°C.
- Temperature normale en vol : 400 a 480°C.
- Limite maximale : 500°C en continu, 550°C transitoire.
- Depassement : extinction moteur possible, risque de dommages turbine.

**Mesure 2 : Temperature huile moteur**

- Graduation : en °C x 10.
- Note du manual : la plage verte depend du type d'huile utilise.
  - Huile AIR 3515A : limite 0 a 35°C (arc vert).
  - Huile DED 2480 : limite 0 a 55°C (arc vert).
- Hors plage verte : reduire la puissance et surveiller.

**Mesure 3 : Pression huile moteur**

- Graduation : 0 a 0,8 bar.
- Pression normale : 0,5 a 0,7 bar en vol.
- Limite basse : < 0,3 bar - alerte immediate, arreter le moteur.

**Implementation simulateur :**

```
temperature_tuyere  : float  # 0 a 550°C
temperature_huile   : float  # °C
pression_huile      : float  # 0 a 0,8 bar
seuil_alerte_huile  : 0.3    # bar
seuil_alerte_tuyere : 500    # °C
```

---

### 2.2 Indicateur de carburant (item 47 - "Fuel contents gauge")

- Graduation : 0 a 15 (US gallons x 10), soit 0 a 150 gallons = 580 litres environ.
- Voyant d'alerte bas carburant : allumage a 4 US gallons (environ 15 litres).
- Consommation indicative : 110 kg/h a vitesse economique, 155 kg/h a puissance maxi.
- Autonomie approximative : 4 h a vitesse economique.

---

### 2.3 Manometre de pression huile de transmission (item 6)

- Voyant rouge uniquement, pas de cadran analogique separe.
- S'allume en cas de chute de pression huile de la boite de transmission principale.
- Alerte critique : atterrissage immediat requis.

---

### 2.4 Jauge de pression servo-commandes (item 13 - "Servo unit pressure gauge")

- Optionnel, present sur les appareils equipes de servo-commandes (a partir du n° 285).
- Graduation non precisee dans les images disponibles.
- Absence de pression : commandes en effort direct (cas du SE 3130 de base sans servo).

---

## 3. Voyants d'alerte et signalisation

Tous les voyants suivants sont references dans la legende officielle (image 2).

| Item | Designation officielle | Couleur | Condition d'allumage |
|---|---|---|---|
| 5 | Red light (alarme generale) | Rouge | Anomalie critique non specifique |
| 6 | Transmission oil pressure warning | Rouge | Chute pression huile transmission |
| 9 | Fuel pressure warning | Rouge | Chute pression carburant |
| 17 | Starter relay warning light | Rouge | Anomalie relais demarreur |
| 19 | Starter warning light | Vert | Demarreur en cours d'utilisation |
| 20 | Micropump warning light | Orange | Pompe micropression en defaut |
| 36 | Fuel filter clogging warning | Variable | Filtre carburant obstrue |
| 39 | Maximum fuel flow warning light | Rouge | Debit carburant maxi atteint |
| 48 | Generator warning light | Rouge | Generateur hors service |
| 66 | Pitot head heating indicator | Variable | Optionnel |

---

## 4. Commandes de puissance et carburant (partie basse du tableau)

### 4.1 Levier de debit carburant (item 68 - "Fuel flow control lever")

- Commande directe du debit d'alimentation de la chambre de combustion.
- Position arriere = debit coupe (extinction).
- Position avant = plein debit.
- Utilise en complement du regulateur automatique.

### 4.2 Levier du regulateur (item 69 - "Governor control lever")

- Regule automatiquement le regime turbine pour maintenir le regime rotor constant.
- Position nominale : le regulateur maintient 33 000 a 34 000 tr/min turbine.
- La vitesse de rotation de l'ensemble turbine-rotor est maintenue constante par
  le regulateur de vitesse du turbomoteur, quelle que soit la charge collective.

### 4.3 Levier de coupure carburant (item 70 - "Fuel shut-off cock control lever")

- Coupure totale de l'alimentation carburant.
- Position fermee : extinction turbine (utilisee en fin de procedure d'arret).
- Distinct du robinet electrique carburant (commande separee, item 2 de la procedure).

### 4.4 Robinet electrique carburant (consigne DGAC 1996)

- Non represente comme levier physique visible mais essentiel en procedure.
- Interrupteur electrique commandant l'ouverture/fermeture du robinet principal.
- Verification obligatoire du micro-switch a chaque mise en route (CN F-1996-072-053).

---

## 5. Commutateurs et disjoncteurs

### 5.1 Commutation electrique principale

| Item | Designation | Type |
|---|---|---|
| 15 | Voltmetre | Instrument |
| 16 | Booster pump switch | Interrupteur |
| 18 | Engine selector switch | Selecteur |
| 22 | 24V receptacle circuit breaker | Disjoncteur |
| 23 | UV lighting circuit breaker | Disjoncteur |
| 24 | Emergency lighting circuit breaker | Disjoncteur (optionnel) |
| 25 | Landing light circuit breaker | Disjoncteur (optionnel) |
| 50 | Generator ON-OFF switch | Interrupteur |
| 51 | Battery master switch | Interrupteur |
| 55 | Instrument panel circuit breaker | Disjoncteur |
| 56 | Starter circuit breaker | Disjoncteur |

### 5.2 Commutation radio et communications

| Item | Designation | Type |
|---|---|---|
| 31 | VHF radio set | Radio |
| 53 | Radio circuit breaker | Disjoncteur |
| 60 | Radio master switch | Interrupteur |
| 61 | Interphone master switch | Interrupteur |
| 63 | Interphone selector switches | Selecteur |
| 57 | Interphone rheostat | Rheostat |

### 5.3 Commutation eclairage

| Item | Designation |
|---|---|
| 26 | UV lighting rheostat |
| 27 | Cabin light circuit breaker |
| 28 | Emergency lighting rheostat |
| 58 | Cabin lighting rheostat |
| 29 | Position lights switch |
| 30 | Anti-collision lights switch (optionnel) |

### 5.4 Commutation equipements optionnels

| Item | Designation |
|---|---|
| 33 | Emergency flotation gear switch |
| 34 | Heating blankets switch |
| 35 | Servo unit 2-position control cock (optionnel) |
| 59 | Spray equipment or hoist master switch |
| 62 | Fuel jettison master switch |
| 64 | Lateral trim cock |

---

## 6. Receptacles et prises

| Item | Designation |
|---|---|
| 14 | 24-volt receptacle |
| 32 | 24-volt receptacle switch (de-froster, etc.) |
| 43 | Flotation gear receptacle |

---

## 7. Commandes de vol (pedestal bas)

| Item | Designation | Remarque |
|---|---|---|
| 67 | Pitot head heating control switch | Optionnel |
| 68 | Fuel flow control lever | Voir section 4.1 |
| 69 | Governor control lever | Voir section 4.2 |
| 70 | Fuel shut-off cock control lever | Voir section 4.3 |
| 71 | Adjustable stop | Butee reglable du levier |

---

## 8. Affichage et visiere

| Item | Designation |
|---|---|
| 1 | Airspeed limitation placard |
| 2 | Compass visor |
| 4 | Instrument panel visor and lighting |
| 11 | Oil temp. placard |
| 37 | Outside air temp. indicator |
| 38 | UV light |
| 65 | UV light |
| 49 | Flap for engine power check diagram |

---

## ANNEXE - Instruments et commandes necessaires au simulateur Artouste

Cette annexe definit strictement ce qui doit etre implemente dans le HUD et le
systeme de commandes du simulateur pour un vol complet et realiste.
Tout le reste (radio, eclairage, optionnels) est hors scope de la version de base.

---

### A1. HUD - Instruments affichables en temps reel

#### Priorite 1 : indispensables au vol

| Instrument | Variable interne | Plage | Zones colorees |
|---|---|---|---|
| Tachymetre rotor | `tachymetre_rotor` | 0 a 420 tr/min | Vert : 340-380 / Rouge : > 400 |
| Tachymetre turbine | `tachymetre_turbine` | 0 a 35 000 tr/min | Vert : 33 000-34 000 / Rouge : > 35 000 |
| Vitesse air | `vitesse_air_kt` | 0 a 140 kt | Vert : 95-105 / Jaune : 105-120 / Rouge : > 120 |
| Altimetre | `altitude_m` | 0 a 5 000 m | Aucune |
| Variometre | `taux_montee_ms` | -15 a +15 m/s | Aucune |
| Collectif | `collectif` | 0 a 100 % | Aucune |
| Cap | `cap_degres` | 0 a 360° | Aucune |

#### Priorite 2 : securite moteur

| Instrument | Variable interne | Plage | Seuil d'alerte |
|---|---|---|---|
| Temperature tuyere | `temperature_tuyere` | 0 a 550°C | Voyant rouge > 500°C |
| Pression huile moteur | `pression_huile` | 0 a 0,8 bar | Voyant rouge < 0,3 bar |
| Carburant | `carburant_litres` | 0 a 580 L | Voyant orange < 15 L |

#### Priorite 3 : information complementaire

| Instrument | Variable interne | Remarque |
|---|---|---|
| Pas collectif en degres | `pas_collectif_degres` | Derive de `collectif` x 14,5 |
| Phase de vol | `phase_vol` | Texte : ARRET, DEMARRAGE, EN VOL, etc. |
| Frein rotor | `frein_rotor` | Indicateur booleen (SERRE / LIBRE) |

---

### A2. Voyants d'alerte a implementer

| Voyant | Variable declenchante | Couleur | Condition |
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

### A3. Commandes de vol a implementer (recap)

#### Axes de vol (analogiques, manette Xbox)

| Commande | Variable | Plage | Entree |
|---|---|---|---|
| Cyclique lateral | `cyclique_x` | -1,0 a +1,0 | Stick gauche X |
| Cyclique longitudinal | `cyclique_y` | -1,0 a +1,0 | Stick gauche Y |
| Palonnier | `palonnier` | -1,0 a +1,0 | Stick droit X |
| Collectif | `collectif` | 0,0 a 1,0 | RT - LT |

#### Commandes de sequence (numeriques, boutons ou clavier)

| Commande | Variable | Entree manette | Touche AZERTY |
|---|---|---|---|
| Batterie | `batterie_on` | X | 1 |
| Robinet carburant | `robinet_carburant` | Y (groupe) | 2 |
| Pompe de gavage | `pompe_gavage` | Y (groupe) | 3 |
| Interrupteur demarrage | `demarrage_on` | A | 4 |
| Frein rotor | `frein_rotor` | B | 5 |

---

### A4. Logique d'affichage HUD recommandee

```
Disposition suggeree (vue cockpit, coins d'ecran) :

  [CAP 270]                              [ALT 1250 m]
  [IAS  85 kt]                           [VSI +2,5 m/s]

  [ROTOR  360 tr/min |||||||||| ]
  [TURBINE 33500 tr/min ======= ]
  [COLLECTIF 55 % ======        ]

  [TMP 445°C]  [HUILE 0,6 bar]  [FUEL 280 L]

  [PHASE : EN VOL]               [FREIN : LIBRE]

  [!] VOYANTS : zone d'alerte en bas au centre, fond rouge si critique
```

#### Regles d'affichage

- En phase ARRET ou DEMARRAGE : afficher prioritairement les parametres moteur
  (tachymetre turbine, tachymetre rotor, frein rotor, phase).
- En vol : masquer frein rotor et phase, afficher tous les instruments de vol.
- Voyants d'alerte : toujours visibles en surimpression, quelle que soit la phase.
- Collectif : barre de progression horizontale en % suffit ; pas de cadran circulaire.
- Tachymetres : cadrans analogiques ou barres de progression avec zones colorees.

---

### A5. Valeurs de reference pour la calibration

| Parametre | Valeur de reference | Usage |
|---|---|---|
| Decollage (collectif min.) | 40 a 55 % / 6 a 8° | En conditions standard, masse nominale |
| Vitesse economique | 85 a 90 kt | Consommation minimale |
| Vitesse de croisiere normale | 95 a 105 kt | Zone verte IAS |
| VNE | 120 kt | Ne pas depasser |
| Regime rotor nominal | 360 tr/min | Maintenu par le regulateur |
| Regime turbine nominal | 33 000 a 34 000 tr/min | Maintenu par le regulateur |
| Temperature tuyere croisiere | 420 a 460°C | Normal |
| Consommation croisiere | 110 kg/h | A 90 kt, masse nominale |
| Autonomie | environ 4 h | Reservoir plein, vitesse eco. |
