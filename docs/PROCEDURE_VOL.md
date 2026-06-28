# PROCEDURE_VOL.md - Alouette II SE 3130 / SA 313B

Procédure simplifiée pour le simulateur. Version pour la turbine Artouste IIC.
Sources : documents techniques helimat.free.fr, consignes DGAC/EASA, témoignages
pilotes PPRuNe, manuel FlightGear FGMEMBERS/Alouette-II.

> AVERTISSEMENT : ce document est une reconstitution simplifiée à visée
> simulateur, non un manuel de vol certifié. Ne pas utiliser sur appareil réel.

---

## Données de référence

| Paramètre | Valeur |
|---|---|
| Turbine | Turbomeca Artouste IIC |
| Régime turbine au ralenti | 19 000 tr/min |
| Régime turbine nominal (rotor engagé) | 33 000 - 34 000 tr/min |
| Régime rotor principal nominal | 360 tr/min |
| Rotor principal | Tripale, 10,20 m de diamètre |
| Rotor anticouple | Bipale, 1,80 m de diamètre |
| Transmission | Roue libre (freewheel) - engagement rotor automatique au-delà d'un seuil de régime turbine (simplification simulateur) |
| Masse à vide | 877 kg |
| MTOW | 1 500 - 1 600 kg selon génération |
| Vitesse de croisière max | 180 km/h |
| Autonomie | 4 h environ à vitesse économique |

---

## PHASE 1 - Démarrage

### 1.1 Vérifications avant démarrage

- Zone dégagée autour de l'appareil (rayon rotor + marge).
- Manette des gaz / débit carburant : FERMÉE (position arrière).
- Interrupteur de démarrage : OFF.
- Interrupteur batterie : OFF.

### 1.2 Mise sous tension

1. Interrupteur batterie : ON.
   - Vérifier tension batterie suffisante (minimum 24 V, idéal 28-29 V).
   - Une batterie faible empêche le démarrage : 3 démarrages successifs assurés
     par la batterie SAFT 35 Ah d'origine.

### 1.3 Démarrage turbine

2. Robinet carburant électrique : OUVERT.
3. Pompe de gavage : ON (laisser tourner environ 20 secondes avant la mise en route).
4. Interrupteur de démarrage : ON (position "Marche").
   - Le démarreur électrique entraîne la turbine en rotation.
   - Surveiller la montée en régime : sifflement compresseur croissant.
   - Vers 10 000 - 12 000 tr/min : allumage automatique, claquement d'ignition,
     flamme en chambre de combustion.
   - La turbine monte vers le ralenti à environ 19 000 tr/min.
   - Durée totale du démarrage turbine : 20 à 30 secondes.

5. Vérification robinet carburant électrique (Consigne DGAC 1996) :
   - Avancer lentement la manette des gaz jusqu'en butée.
   - Mettre l'interrupteur de démarrage sur OFF.
   - La turbine ne doit PAS s'éteindre (vérification du micro-switch 2/3).
   - Remettre l'interrupteur sur ON.
   - Ramener la manette des gaz en position ralenti.

### 1.4 Engagement du rotor

Sur le SE 3130 Artouste IIC, la turbine est reliée au rotor par une roue libre (freewheel),
sans embrayage centrifuge. Dans le simulateur, l'engagement du rotor est simplifié :
le rotor s'engage automatiquement dès que la turbine dépasse un seuil de régime,
sans action du pilote sur le frein rotor.

6. Rotor : engagement automatique au-delà du seuil de régime turbine.
   - Le rotor commence à tourner lentement, s'accélère progressivement.
   - "Whomp whomp" lents qui s'accélèrent sur 30 à 60 secondes.
   - Le régime turbine monte de 19 000 tr/min (ralenti sans charge) vers
     33 000 - 34 000 tr/min (régime nominal avec rotor engagé).
   - Le rotor principal atteint 360 tr/min.

7. Stabilisation au régime nominal : attendre 1 à 2 minutes avant décollage.

---

## PHASE 2 - Vol

### 2.1 Décollage

- Collective : augmenter progressivement le pas collectif.
- Le pas du rotor augmente, la puissance turbine augmente en conséquence
  (le régulateur maintient le régime rotor constant à 360 tr/min).
- Effet de sol sensible jusqu'à environ 5 m de hauteur (demi-diamètre rotor).
- Translation lift : gain de portance notable dès 15 kt de vitesse sol.

### 2.2 Vol de croisière

Paramètres habituels en vol stabilisé :

| Paramètre | Valeur typique |
|---|---|
| Régime turbine | 33 000 - 34 000 tr/min (constant, géré par le régulateur) |
| Régime rotor principal | 360 tr/min (constant) |
| Vitesse économique | 90 km/h |
| Vitesse croisière normale | 150 - 170 km/h |
| Vitesse max | 185 km/h (ne pas dépasser en vol en ligne droite) |
| Consommation à 90 km/h | 110 kg/h |
| Consommation à puissance maxi | 155 kg/h |

Comportement en vol :
- Pas de servocommandes hydrauliques sur les commandes de vol : efforts directs,
  sensibilité aux turbulences.
- Palonnier : agit directement sur le pas du rotor anticouple.
- Manche cyclique : inclinaison du plateau cyclique.
- Collective : variation de pas collectif sur le rotor principal.

### 2.3 Atterrissage

- Réduire la vitesse d'approche progressivement.
- Descente : maintenir un taux de descente modéré pour éviter le vortex ring state
  (ne pas descendre verticalement à fort taux avec faible vitesse sol).
- Effet de sol en finale sous 5 m : portance accrue, réduire la collective.
- Poser en douceur, sans hésitation.

---

## PHASE 3 - Arrêt

### 3.1 Mise au sol et stabilisation

1. S'assurer que l'appareil est bien posé, pas de roulis, pas de lacet parasite.
2. Collective : mise à zéro (pas collectif minimum).

### 3.2 Refroidissement turbine avant arrêt

3. Maintenir le ralenti turbine (19 000 tr/min) pendant 1 à 2 minutes.
   Objectif : refroidir la chambre de combustion avant coupure carburant.
   Ne pas couper la turbine brutalement après un vol exigeant.

### 3.3 Arrêt turbine

4. Manette des gaz / débit carburant : FERMÉE (couper l'alimentation).
   Ou : interrupteur de démarrage sur OFF (fermeture du robinet électrique).
   - Extinction flamme chambre de combustion.
   - Sifflement compresseur qui descend progressivement en fréquence.
   - Le rotor ralentit progressivement par la roue libre, puis s'arrête.
   - "Whomp whomp" qui s'espacent, battement de plus en plus lent.
   - Durée arrêt rotor : 20 à 40 secondes.
   - Le compresseur tourne en roue libre quelques secondes avant l'arrêt complet.
   - Durée arrêt turbine : 10 à 20 secondes.

### 3.4 Coupures finales

5. Robinet carburant électrique : FERMÉ.
6. Pompe de gavage : OFF.
7. Interrupteur batterie : OFF.
8. Vérifier arrêt complet rotor et turbine avant de quitter l'appareil.

---

## Résumé séquentiel (pour intégration simulateur)

```
DÉMARRAGE
  batterie ON
  robinet carburant OUVERT
  pompe de gavage ON (20 s)
  interrupteur démarrage ON
    -> sifflement turbine croissant (~20 s)
    -> allumage (~12 000 tr/min)
    -> ralenti 19 000 tr/min atteint
  vérification micro-switch 2/3
    -> engagement rotor automatique au seuil de régime (roue libre)
    -> rotor accélère (~45 s)
    -> régime nominal : turbine 33-34 000 tr/min, rotor 360 tr/min
  stabilisation 1-2 min
  PRÊT AU DÉCOLLAGE

VOL
  collective croissante -> décollage
  vol stabilisé : régime constant, pilotage à effort direct
  atterrissage : réduction progressive, poser sans hésitation

ARRÊT
  collective à zéro
  ralenti 1-2 min (refroidissement)
  coupure carburant (touche T)
    -> rotor ralentit par la roue libre (20-40 s)
    -> sifflement turbine descendant (10-20 s)
  robinet FERMÉ
  pompe OFF
  batterie OFF
```

---

## Notes pour le simulateur

### Synchronisation sons / état moteur

| Événement | Son associé | Durée |
|---|---|---|
| Interrupteur démarrage ON | demarrage_turbine_ext/int | 20-30 s |
| Frein rotor desserré | demarrage_pales_ext/int | 30-60 s |
| Régime nominal atteint | -> bascule sur vol_ext/int (boucle) | continu |
| Frein rotor serré | arret_pales_ext/int | 20-40 s |
| Coupure carburant | arret_turbine_ext/int | 10-20 s |

### Paramètres d'animation rotor

| Rotor | Régime au sol (avant frein) | Régime ralenti (sans charge) | Régime nominal |
|---|---|---|---|
| Principal | 0 tr/min | engagé progressivement | 360 tr/min |
| Anticouple | 0 tr/min | proportionnel au principal | proportionnel |

La transition de 0 à 360 tr/min au démarrage prend 30 à 60 secondes.
La transition inverse à l'arrêt (avec frein) prend 20 à 40 secondes.

### Effets de vol modélisés

Au-delà de l'effet de sol et de l'effet de translation, le simulateur reproduit
quatre comportements liés au domaine de vol de l'Alouette II. Tous restent
volontairement simplifiés, mais ils en respectent l'esprit.

| Effet | Comportement dans le simulateur |
|---|---|
| Densité de l'air | La portance et la puissance de la turbine décroissent avec l'altitude. Au-delà d'environ 1 900 m à masse nominale, le stationnaire hors effet de sol devient impossible : l'appareil descend même collectif à fond. C'est le comportement documenté du SE 3130, premier hélicoptère de haute montagne. |
| VNE variable | La vitesse à ne pas dépasser vaut 105 kt au niveau de la mer et diminue avec l'altitude. Au-delà, une traînée d'onde croissante freine l'appareil et matérialise la limite. |
| Vol latéral ou arrière | Limité à 18 kt. Au-delà, le rotor anticouple sature et l'autorité au palonnier diminue, comme sur l'appareil réel. |
| Vortex ring state | En descente verticale rapide (plus de 3 m/s) à faible vitesse sol et à puissance partielle, le rotor retombe dans son propre souffle et perd de la portance. On s'en dégage en accélérant vers l'avant (cyclique en avant). |

Ces quatre difficultés sont désactivées en mode assisté et pendant la démo : le vol
y reste facile et le parcours de démonstration prévisible. Les effets qui aident le
pilote (effet de sol, effet de translation) restent actifs dans tous les cas.

En vue cockpit, une vibration légère de la cabine reproduit les trois impulsions
par tour du rotor tripale (3/rev, environ 18 Hz au régime nominal). Cet effet est
purement visuel et n'intervient pas dans le calcul du vol.

---

## ANNEXE - Commandes minimales pour un premier vol simulateur

Cette annexe définit le jeu de commandes strictement nécessaire pour effectuer
un vol complet dans le simulateur, du démarrage à l'arrêt. Aucune autre entrée
n'est requise pour valider le vol de base.

### Commandes de pilotage (axes de vol)

| Commande | Axe | Effet | Entrée physique suggérée |
|---|---|---|---|
| Manche cyclique longitudinal | Tangage | Cabré / piqué | Axe Y joystick ou stick |
| Manche cyclique latéral | Roulis | Inclinaison gauche / droite | Axe X joystick ou stick |
| Palonnier gauche / droit | Lacet | Anticouple, orientation | Axe Z joystick, pédales, ou touches clavier |
| Levier de pas collectif | Altitude / puissance | Montée / descente | Axe throttle ou touches clavier |

> Sur l'Alouette II, les quatre axes sont indissociables : toute action collective
> modifie le lacet (compensation palonnier nécessaire), et toute accélération
> vers l'avant nécessite un léger cabré puis une inclinaison cyclique.

### Commandes de séquence démarrage / arrêt

Ces commandes déclenchent les états internes du simulateur et les sons associés.
Chacune peut être mappée sur une touche clavier, un bouton ou un élément de cockpit 3D.

| Commande | État déclenché | Son associé |
|---|---|---|
| Batterie ON | Mise sous tension | Bip voyants |
| Robinet carburant OUVERT | Alimentation moteur active | - |
| Pompe de gavage ON | Pressurisation circuit | Ronronnement pompe |
| Interrupteur démarrage ON | Lancement séquence turbine | demarrage_turbine |
| Frein rotor DESSERRÉ | Engagement rotor | demarrage_pales |
| Frein rotor SERRÉ | Arrêt rotor | arret_pales |
| Coupure carburant | Extinction turbine | arret_turbine |
| Batterie OFF | Coupure générale | - |

### Mapping clavier minimal suggéré (à adapter selon moteur de jeu)

| Touche | Action |
|---|---|
| Z / S | Cyclique avant / arrière (tangage) |
| Q / D | Cyclique gauche / droite (roulis) |
| A / E | Palonnier gauche / droite (lacet) |
| Molette souris ou + / - | Collectif monter / descendre |
| F1 | Batterie ON/OFF |
| F2 | Robinet carburant OUVERT/FERMÉ |
| F3 | Pompe de gavage ON/OFF |
| F4 | Interrupteur démarrage ON/OFF |
| F5 | Frein rotor SERRÉ/DESSERRÉ |
| Échap | Pause / quitter |

> Si un joystick est disponible, mapper les quatre axes de vol en priorité.
> Les commandes de démarrage restent sur clavier ou cockpit 3D.

### États internes minimaux du simulateur

Le simulateur doit maintenir au minimum les variables d'état suivantes
pour que la séquence démarrage-vol-arrêt soit cohérente :

```
batterie_on          : bool
robinet_carburant    : bool
pompe_gavage         : bool
demarrage_on         : bool
frein_rotor          : bool   # true = serré

regime_turbine       : float  # tr/min, 0 à 34 000
regime_rotor         : float  # tr/min, 0 à 360

phase_vol            : enum   # ARRET | DEMARRAGE_TURBINE | DEMARRAGE_ROTOR
                              #       | EN_VOL | ARRET_ROTOR | ARRET_TURBINE

# Axes de vol (normalisés -1.0 à +1.0)
cyclique_x           : float  # roulis
cyclique_y           : float  # tangage
collectif            : float  # 0.0 à 1.0
palonnier            : float  # lacet
```

### Séquence minimale de vol valide

```
1. batterie_on = true
2. robinet_carburant = true
3. pompe_gavage = true
4. demarrage_on = true
   -> phase = DEMARRAGE_TURBINE
   -> regime_turbine monte vers 19 000 (~25 s)
5. frein_rotor = false
   -> phase = DEMARRAGE_ROTOR
   -> regime_rotor monte vers 360 (~45 s)
   -> regime_turbine monte vers 34 000
   -> phase = EN_VOL quand regime_rotor >= 355
6. [pilotage libre via les quatre axes]
7. collectif = 0
8. frein_rotor = true
   -> phase = ARRET_ROTOR
   -> regime_rotor descend vers 0 (~30 s)
9. demarrage_on = false (ou robinet_carburant = false)
   -> phase = ARRET_TURBINE
   -> regime_turbine descend vers 0 (~15 s)
   -> phase = ARRET
10. batterie_on = false
```

---

## ANNEXE 2 - Commandes implémentées dans le simulateur Artouste

Tableau de référence des commandes telles qu'elles existent dans le code du simulateur.
Source : tableau fourni par le développeur. Ne pas modifier sans mise à jour du code.

---

### Tableau complet des commandes

| Action | Clavier AZERTY | Manette Xbox |
|---|---|---|
| Collectif + | W ou Z | Gâchette RT |
| Collectif - | S | Gâchette LT |
| Cyclique (4 directions) | Flèches directionnelles | Stick gauche |
| Palonnier gauche | A ou Q | Stick droit (axe X, gauche) |
| Palonnier droit | D | Stick droit (axe X, droite) |
| Turbine démarrer / couper | T | Bouton Start |
| Vue (poursuite / cockpit / orbite) | C | Bouton Y (jaune) |
| HUD on / off | H | Bouton B |
| Pause | P | Bouton Back |
| Réinitialiser la position | R | Bouton X |
| Quitter | Échap | LB + RB simultanés |

---

### Notes d'implémentation

**Collectif :** deux touches distinctes sur clavier (W/Z pour monter, S pour descendre),
deux gâchettes analogiques sur manette (RT = monter, LT = descendre). En mode manette,
la valeur du collectif est continue (0,0 à 1,0) ; en mode clavier, elle est incrémentale
(paliers discrets à chaque appui).

**Cyclique :** flèches directionnelles sur clavier (binaire), stick gauche analogique
sur manette. Le stick gauche couvre les deux axes simultanément (tangage + roulis).

**Palonniers :** deux touches séparées sur clavier (A/Q = gauche, D = droite),
axe horizontal du stick droit sur manette.

**Turbine :** commande unique T / Start qui déclenche la séquence de démarrage ou d'arrêt
selon l'état courant de la phase de vol.

**Vue :** la touche C et le bouton Y cyclent entre les modes de caméra disponibles
(poursuite, cockpit, orbite).

**Commandes sans équivalent manette :** HUD (H), pause (P), reset (R) et quitter (Échap)
sont exclusivement sur clavier. À prévoir dans l'interface si un mode manette seule
est souhaité à terme.

---

### Correspondance avec les phases de vol

| Phase | Commandes actives | Commandes inactives |
|---|---|---|
| ARRET (avant démarrage) | T (démarrage turbine) | Cyclique, palonnier, collectif |
| DEMARRAGE_TURBINE | - (séquence automatique) | Tout |
| DEMARRAGE_ROTOR | - (séquence automatique) | Tout |
| EN_VOL | Collectif, cyclique, palonnier, T (coupure), C, H, P, R | - |
| ARRET_ROTOR | - (séquence automatique) | Tout |
| ARRET_TURBINE | - (séquence automatique) | Tout |

---

### Initialisation recommandée au lancement

Au démarrage du simulateur, avant toute action :

- Collectif : 0 (pas de puissance).
- Cyclique : centré (position neutre).
- Palonnier : centré (position neutre).
- Phase : ARRET.
- HUD : visible par défaut.

Prévoir une zone morte (dead zone) de 10 % sur tous les axes analogiques de la manette
pour compenser le jeu mécanique des sticks Xbox.
