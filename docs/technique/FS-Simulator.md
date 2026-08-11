# FS-Simulator : points à traiter dans le simulateur

Source : `FS-Simulator.pdf` (FSHeli.ch, planches du manuel de vol allemand de
l'Alouette II Artouste II C). Étude du 11/08/2026. Fichiers visés :
`src/physics/constants.hpp` et `src/physics/FlightModel.cpp`, sur main.
Aucun lien avec la branche aspect-modele.

## Vérifications préalables

1. FAIT (11/08/2026). Courbes de la page 1 numérisées (montée à 90 km/h,
   34 000 tr/min, altitude-densité, +/-0,15 m/s) :

   | masse   | 0 m  | 1000 m | 2000 m | 3000 m | 4000 m | plafond Vz=0 |
   |---------|------|--------|--------|--------|--------|--------------|
   | 1100 kg | 8,10 | 7,00   | 5,95   | 5,10   | 3,85   | > 4500 m     |
   | 1200 kg | 7,20 | 6,15   | 5,15   | ~4,0   | 2,85   | > 4500 m     |
   | 1300 kg | 6,40 | 5,35   | 4,35   | 3,10   | 1,95   | ~4450 m      |
   | 1400 kg | 5,55 | 4,62   | 3,55   | 2,08   | ~0,2   | ~3950 m      |
   | 1500 kg | 4,89 | 3,87   | 2,78   | 0,55   | -      | ~3050 m      |
   | 1600 kg | 4,15 | 3,10   | 1,95   | -      | -      | ~2300 m      |

   Pente commune ~-1,05 m/s par 1000 m, puis effondrement (coude) ~2 m/s
   au-dessus du plafond. La lecture initiale (1 à 2 m/s sous heli-archive)
   est réfutée : la planche recoupe heli-archive à 0,1 m/s près (5,98 à
   1350 kg contre 6,0 ; 4,89 à 1500 contre 5,0) et le plafond 1600 kg
   (2300 m) recoupe le plafond pratique constructeur. Écart réel : à
   1100 kg la planche donne 8,1 m/s là où le modèle prédit 9,4 (pente
   masse/montée du manuel ~0,79 m/s par 100 kg, bilan de puissance ~1,0).
2. FAIT (11/08/2026, calibration contrôlée sur la ligne ISA imprimée).
   Plafonds de stationnaire en
   atmosphère normale, +/-50 m (* = extrapolé hors cadre, +/-150 à 200 m) :

   | masse   | DES (1,5 m) | HES     |
   |---------|-------------|---------|
   | 1000 kg | -           | ~4820 m*|
   | 1100 kg | ~4700 m*    | 4070 m  |
   | 1200 kg | 3930 m      | 3230 m  |
   | 1300 kg | 3150 m      | 2410 m  |
   | 1400 kg | 2380 m      | 1720 m  |
   | 1500 kg | 1700 m      | 1060 m  |
   | 1600 kg | 1020 m      | 420 m   |

   La lecture initiale (3700-3800 m à 1100 kg HES) est réfutée : 4070 m.
   L'effet de sol à 1,5 m équivaut à 85-95 kg. Pente des courbes -28 à
   -30 m par deg C. Masse maxi au stationnaire ISA niveau de la mer :
   ~1665 kg HES, ~1750 kg DES. L'écart avec le bilan actuel (excédent
   annulé vers 3000 m à 1100 kg) est confirmé et vaut ~1000 m.

## Modifications selon le résultat

3. FAIT (11/08/2026). Régime transitoire par plancher de puissance
   (`POWER_FLAT_W` 152,5 kW, calage RASANT : 1 kW = ~577 m de plafond),
   durée portée par la surchauffe de tuyère ; plafond HES mesuré à 4078 m,
   pris plein levier (15 deg), la fonte du plancher le régulant au-delà ;
   `AIR_DENSITY_SCALE` 5500 -> 7200, mur de sustentation 4304 m.
4. FAIT (11/08/2026). Absorption liée au pas (`ABSORPTION_PENTE`
   0,0596/deg) : 8,10 m/s à 14 deg / 90 km/h au niveau de la mer,
   décroissance conforme à la planche à +/-0,15 m/s ; plein levier, palier
   et points 1350/1500/1600 inchangés.

## Modifications sans préalable

5. FAIT (11/08/2026). Bandeau HUD discret ZONE H-V (`hvIntensity`, lissage
   2 s, éteint turbine coupée ou au sol) + entrée dans la page pilotes de la
   notice.
6. FAIT (11/08/2026). Levier gradué 6-15 deg de pas réel, stationnaire à
   11 deg, butée élastique 14,5 (jaune au HUD, coin et cadran PAS),
   secours 15.
7. FAIT (11/08/2026). TMP asservie à la loi pas/température de la page 7
   (`T4_LOI_*`), transitoire vers 550 ; `Turbine::update` prend une cible en
   deg C.
8. FAIT (11/08/2026). Le HUD affiche 34 000 tr/min en puissance
   (`ApplicationHudInstruments.cpp`, `ApplicationCapture.cpp`) ; LED
   inchangée, la bande verte du cadran est déjà 33 000-34 000.

## Rien à changer, validé par le document

- VNE : plateau 195 km/h puis 20 km/h perdus par 1000 m, conforme à la table
  page 5 pour 1100 kg (`VNE_SEA_LEVEL_MS`, `VNE_ALT_PLATEAU`, `VNE_ALT_PENTE`).
- Consommation 112 à 194 L/h : dans la carte moteur (page 2) pour la plage de
  puissance utilisée par le modèle.
- TMP à 445 deg C : dans la plage du manuel.

## Hors code

9. FAIT (11/08/2026). PDF archivé sous
   `artouste-sources-tierces/docs-se3130/fsheli-alouette2-reference.pdf` ;
   source et données numérisées ajoutées à `references-se3130.json`
   (montée à 90 km/h, plafonds DES/HES) et entrée bibliographique dans
   `REFERENCES.md`. Reconstitution typographique des dix planches avec
   figures : `docs/technique/planches-manuel-vol.tex` (.pdf compilé).
