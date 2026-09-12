# Note technique : sources documentaires d'époque, structuration et analyse des données de référence

Projet : Artouste (simulateur Alouette II SE 3130, C++20 / OpenGL 3.3)
Destinataire : Claude Code, pour la collecte, le stockage et l'analyse des données de performance de référence

## Objectif

Cette note recense les sources documentaires d'époque et actuelles disponibles pour les caractéristiques du SE 3130 / SE 313B, signale les divergences constatées entre elles, et propose une structure de données ainsi qu'une méthode d'analyse pour que le moteur physique puisse être calibré et validé contre plusieurs sources plutôt que contre une valeur unique supposée exacte.

## Constat : les sources divergent, y compris entre sources sérieuses

Deux compilations sérieuses donnent des chiffres différents pour le même appareil (SE 3130 / SE 313B, masse maximale 1 600 kg, niveau de la mer) :

| Grandeur | Jane's All The World's Aircraft 1966-67 | ALAT / ESAM (Malcros, 2018) |
|---|---|---|
| Vitesse de croisière | 170 km/h | 160 km/h |
| Vitesse maximale | 185 km/h | 170 km/h |
| Vitesse ascensionnelle | 4,2 m/s (820 ft/min) | 4,4 m/s |
| Plafond pratique | 2 300 m | 3 200 m |

Cet écart dépasse l'incertitude de mesure attendue et suggère soit des conditions d'essai différentes (température, état de l'appareil, méthode de mesure), soit une confusion entre plusieurs sous-versions, soit une transcription approximative dans l'une des deux sources. Le moteur physique ne doit donc pas être calibré sur une valeur unique présentée comme vérité absolue, mais sur une fourchette de référence documentée, avec la source et les conditions de chaque valeur tracées.

## Sources disponibles, par niveau d'autorité

### Niveau 1 : documents de certification officiels

- **Fiche de navigabilité DGAC n°24** (Direction Générale de l'Aviation Civile, mai 1977), couvrant le SE 310 et le SE 313B Alouette Astazou. C'est le document réglementaire français équivalent au Type Certificate Data Sheet américain : il fait autorité sur les limites structurelles et opérationnelles certifiées (VNE, masses, centrage).
- **Type Certificate Data Sheet FAA n°7H1** (Federal Aviation Administration, 10 janvier 2014), pour la certification américaine de l'appareil. Ce document couvre nommement le SE 3130-Alouette II, le SE 313B-Alouette II, le SA 3180-Alouette Astazou, le SA 318B-Alouette Astazou et le SA 318C-Alouette Astazou. Il constitue une validation américaine directe du certificat de navigabilité DGAC d'origine (délivré le 2 mai 1957) : l'Alouette II a été le premier hélicoptère à turbine certifié aux États-Unis, le 14 janvier 1958, par une procédure d'acceptation bilatérale reposant sur les données de certification françaises. C'est donc, de fait, un document de même niveau d'autorité que la fiche DGAC n°24, et accessible tant que celle-ci reste hors d'atteinte via Avialogs. Accès : portail de recherche du Dynamic Regulatory System de la FAA, https://drs.faa.gov/ (rechercher "7H1" ou "Alouette II" ; aucune URL statique fixe identifiée, le DRS fonctionnant par recherche interne, inscription gratuite parfois nécessaire pour le téléchargement).

### Niveau 2 : documentation constructeur d'époque

- **Manuel de vol Sud-Aviation / SNCASE**, document de référence du pilote en cabine. Existence confirmée, aucune version numérisée librement accessible identifiée à ce jour ; à rechercher auprès du Musée de l'Air et de l'Espace (Le Bourget), du Musée de l'ALAT (Dax), ou de collections privées d'aviation militaire.
- **Manuels de maintenance d'origine** (Sud Aviation / SNCASE / Aérospatiale), numérisés et distribués par des archivistes spécialisés en documentation aéronautique historique : Helicopter Maintenance Manual Volume 1 et 2 (1960), Structural Repair Manual (1970), Tool Catalog Manual (1970). Utiles pour les données structurelles et les limites de composants, moins pour les performances en vol.

### Niveau 3 : compilations opérationnelles et historiques sérieuses

- **Christian Malcros, "Les Aéronefs de l'ALAT" (volume 18), Sud Aviation SE-3130 Alouette II en service dans l'ALAT**, édition octobre 2018, publication gratuite. Données techniques sourcées auprès de Jean-Pierre Cabray et de l'ESAM (École Spécialisée de l'Aviation légère de l'Armée de Terre), soit la structure même qui formait les pilotes et assurait le suivi opérationnel de l'appareil. Contient également l'historique complet de production et d'affectation des cellules, utile pour du contexte mais hors sujet pour la physique de vol.
- **Jane's All The World's Aircraft 1966-67** (Taylor, J. W. R.), compilation généraliste anglo-saxonne largement reprise (Wikipedia, fiches constructeur en ligne). Fiable pour une vue d'ensemble comparative entre appareils, moins ancrée dans l'usage opérationnel français réel.
- **Heli-Archive (heli-archive.ch)**, fiche historique et technique détaillée, utile en particulier pour la comparaison entre variantes Artouste et Astazou (VNE de 205 km/h pour le SA 318C, par exemple).

### Niveau 4 : références génériques (déjà couvertes dans les notes précédentes)

- FAA-H-8083-21B, *Helicopter Flying Handbook* : comportement aérodynamique général, pas de données spécifiques au SE 3130.
- Rapports NASA (NTRS 19830001781, 19850001726, 19800019870) : structure d'équations de simulation, pas de données spécifiques au SE 3130.

## Structure de données proposée pour le stockage

Plutôt que de coder les valeurs de performance en dur dans le moteur physique, stocker les données de référence dans un fichier structuré (JSON ou YAML) versionné avec le code, où chaque valeur porte sa source et ses conditions. Exemple de structure :

```json
{
  "aircraft": "SE 3130 / SE 313B",
  "engine": "Turbomeca Artouste II (IIB1, IIC, IIC1, IIC2, IIC5, IIC6 au certificat 7H1)",
  "reference_values": [
    {
      "quantity": "vitesse_ascensionnelle_max",
      "value": 4.2,
      "unit": "m/s",
      "conditions": {
        "altitude": "niveau de la mer",
        "masse_kg": 1600,
        "vitesse": "VY"
      },
      "source": "Jane's All The World's Aircraft 1966-67",
      "authority_level": 3
    },
    {
      "quantity": "vitesse_ascensionnelle_max",
      "value": 4.4,
      "unit": "m/s",
      "conditions": {
        "altitude": "niveau de la mer",
        "masse_kg": 1600,
        "vitesse": "VY"
      },
      "source": "ALAT / ESAM (Malcros, 2018, via J.-P. Cabray)",
      "authority_level": 3
    },
    {
      "quantity": "VNE",
      "value": 195,
      "unit": "km/h",
      "conditions": { "variante": "SE.3130, Artouste II B1" },
      "source": "Manuel de vol / donnees de certification",
      "authority_level": 1
    }
  ]
}
```

Champs recommandés pour chaque entrée :
- `quantity` : grandeur normalisée (utiliser un nom stable, pas une chaîne libre, pour permettre l'agrégation programmatique).
- `value` et `unit` : toujours en unités SI dans le fichier de stockage ; les conversions d'affichage (km/h, ft/min) se font à la lecture, jamais en dur dans les données sources.
- `conditions` : masse, altitude, vitesse, variante moteur. Une valeur sans conditions précisées n'est pas comparable à une autre.
- `source` : référence bibliographique complète, pas juste un nom de site.
- `authority_level` : 1 (certification officielle) à 4 (référence générique non spécifique à l'appareil), pour pondérer les sources en cas de conflit.

## Ce qui est réellement simulé

Le modèle reproduit les performances publiées de la cellule SE 3130 : taux de
montée, plafond de stationnaire, limites du certificat 7H1, communes aux six
Artouste II certifiées. La puissance au rotor est résolue depuis ces
performances, si bien que le vol rend une Alouette II quelle que soit la turbine
montée, et l'écart entre IIB1 et IIC y reste invisible. La NOTE 6 du certificat
explique pourquoi : les 400 ch sont une limite de transmission, et toutes les
variantes y sont bridées.

## Méthode d'analyse et de validation du moteur physique

1. **Ne jamais valider contre une valeur unique.** Pour chaque grandeur (VNE, Vmax, vario, plafond), calculer la fourchette [min, max] issue des sources de niveau 1 à 3, et considérer le modèle comme cohérent si sa sortie tombe dans cette fourchette, pas seulement s'il colle à une valeur précise.
2. **Pondérer par niveau d'autorité en cas de conflit réel.** Si deux sources de même niveau divergent (cas du vario Jane's vs ALAT/ESAM), ne pas trancher arbitrairement : documenter les deux valeurs, et si une troisième source (idéalement de niveau 1) est trouvée, l'utiliser comme arbitre.
3. **Journaliser chaque comparaison avec ses conditions.** Un test de vario à 1 332 m d'altitude (terrain Ossau) ne se compare pas directement à une référence donnée au niveau de la mer sans correction d'altitude densité. Le protocole de test doit soit reproduire les conditions de la référence, soit appliquer une correction documentée avant comparaison.
4. **Prioriser la recherche de sources de niveau 1 manquantes.** La Fiche de navigabilité DGAC n°24 et le manuel de vol constructeur sont les deux documents qui trancheraient la plupart des divergences actuelles ; leur obtention (contact Musée de l'Air et de l'Espace, Musée de l'ALAT, ou service historique de l'armée de Terre) est la piste la plus rentable pour lever l'incertitude, plus que l'ajustement fin de coefficients sur la base de sources de niveau 3 contradictoires.
5. **Ne pas réconcilier silencieusement.** Si le moteur physique doit choisir une valeur unique pour fonctionner (par exemple pour plafonner la vitesse), documenter explicitement dans le code et dans un commentaire quelle source a été retenue et pourquoi, plutôt que de moyenner arbitrairement deux chiffres d'origines différentes.

## Pistes pour compléter la collecte

- Musée de l'Air et de l'Espace (Le Bourget) : fiche et archives constructeur SNCASE / Sud-Aviation.
- Musée de l'ALAT (Dax) : documentation opérationnelle française, manuels de vol d'unités.
- Service Historique de la Défense : archives de certification et de programme.
- alat.fr et alat2.fr (association Christian Malcros) : autres volumes de la série "Les Aéronefs de l'ALAT", potentiellement d'autres données techniques croisées.
- aircraft-reports.com : manuels de maintenance et réparation structurelle numérisés, payants, utiles pour les données structurelles plutôt que les performances en vol.
