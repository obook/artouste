# Note technique : sources documentaires d'epoque, structuration et analyse des donnees de reference

Projet : Artouste (simulateur Alouette II SE 3130, C++20 / OpenGL 3.3)
Destinataire : Claude Code, pour la collecte, le stockage et l'analyse des donnees de performance de reference

## Objectif

Cette note recense les sources documentaires d'epoque et actuelles disponibles pour les caracteristiques du SE 3130 / SE 313B, signale les divergences constatees entre elles, et propose une structure de donnees ainsi qu'une methode d'analyse pour que le moteur physique puisse etre calibre et valide contre plusieurs sources plutot que contre une valeur unique supposee exacte.

## Constat : les sources divergent, y compris entre sources serieuses

Deux compilations serieuses donnent des chiffres differents pour le meme appareil (SE 3130 / SE 313B, masse maximale 1 600 kg, niveau de la mer) :

| Grandeur | Jane's All The World's Aircraft 1966-67 | ALAT / ESAM (Malcros, 2018) |
|---|---|---|
| Vitesse de croisiere | 170 km/h | 160 km/h |
| Vitesse maximale | 185 km/h | 170 km/h |
| Vitesse ascensionnelle | 4,2 m/s (820 ft/min) | 4,4 m/s |
| Plafond pratique | 2 300 m | 3 200 m |

Cet ecart n'est pas anodin : il depasse largement l'incertitude de mesure attendue et suggere soit des conditions d'essai differentes (temperature, etat de l'appareil, methode de mesure), soit une confusion entre plusieurs sous-versions, soit une transcription approximative dans l'une des deux sources. Le moteur physique ne doit donc pas etre calibre sur une valeur unique presentee comme verite absolue, mais sur une fourchette de reference documentee, avec la source et les conditions de chaque valeur tracees.

## Sources disponibles, par niveau d'autorite

### Niveau 1 : documents de certification officiels

- **Fiche de navigabilite DGAC n°24** (Direction Generale de l'Aviation Civile, mai 1977), couvrant le SE 310 et le SE 313B Alouette Astazou. C'est le document reglementaire francais equivalent au Type Certificate Data Sheet americain : il fait autorite sur les limites structurelles et operationnelles certifiees (VNE, masses, centrage).
- **Type Certificate Data Sheet FAA n°7H1** (Federal Aviation Administration, 10 janvier 2014), pour la certification americaine de l'appareil. Ce document couvre nommement le SE 3130-Alouette II, le SE 313B-Alouette II, le SA 3180-Alouette Astazou, le SA 318B-Alouette Astazou et le SA 318C-Alouette Astazou. Il constitue une validation americaine directe du certificat de navigabilite DGAC d'origine (delivre le 2 mai 1957) : l'Alouette II a ete le premier helicoptere a turbine certifie aux Etats-Unis, le 14 janvier 1958, par une procedure d'acceptation bilaterale reposant sur les donnees de certification francaises. C'est donc, de fait, un document de meme niveau d'autorite que la fiche DGAC n°24, et accessible tant que celle-ci reste hors d'atteinte via Avialogs. Acces : portail de recherche du Dynamic Regulatory System de la FAA, https://drs.faa.gov/ (rechercher "7H1" ou "Alouette II" ; aucune URL statique fixe identifiee, le DRS fonctionnant par recherche interne, inscription gratuite parfois necessaire pour le telechargement).

### Niveau 2 : documentation constructeur d'epoque

- **Manuel de vol Sud-Aviation / SNCASE**, document de reference du pilote en cabine. Existence confirmee, aucune version numerisee librement accessible identifiee a ce jour ; a rechercher aupres du Musee de l'Air et de l'Espace (Le Bourget), du Musee de l'ALAT (Dax), ou de collections privees d'aviation militaire.
- **Manuels de maintenance d'origine** (Sud Aviation / SNCASE / Aerospatiale), numerises et distribues par des archivistes specialises en documentation aeronautique historique : Helicopter Maintenance Manual Volume 1 et 2 (1960), Structural Repair Manual (1970), Tool Catalog Manual (1970). Utiles pour les donnees structurelles et les limites de composants, moins pour les performances en vol.

### Niveau 3 : compilations operationnelles et historiques serieuses

- **Christian Malcros, "Les Aeronefs de l'ALAT" (volume 18), Sud Aviation SE-3130 Alouette II en service dans l'ALAT**, edition octobre 2018, publication gratuite. Donnees techniques sourcees aupres de Jean-Pierre Cabray et de l'ESAM (Ecole Specialisee de l'Aviation legere de l'Armee de Terre), soit la structure meme qui formait les pilotes et assurait le suivi operationnel de l'appareil. Contient egalement l'historique complet de production et d'affectation des cellules, utile pour du contexte mais hors sujet pour la physique de vol.
- **Jane's All The World's Aircraft 1966-67** (Taylor, J. W. R.), compilation generaliste anglo-saxonne largement reprise (Wikipedia, fiches constructeur en ligne). Fiable pour une vue d'ensemble comparative entre appareils, moins ancree dans l'usage operationnel francais reel.
- **Heli-Archive (heli-archive.ch)**, fiche historique et technique detaillee, utile en particulier pour la comparaison entre variantes Artouste et Astazou (VNE de 205 km/h pour le SA 318C, par exemple).

### Niveau 4 : references generiques (deja couvertes dans les notes precedentes)

- FAA-H-8083-21B, *Helicopter Flying Handbook* : comportement aerodynamique general, pas de donnees specifiques au SE 3130.
- Rapports NASA (NTRS 19830001781, 19850001726, 19800019870) : structure d'equations de simulation, pas de donnees specifiques au SE 3130.

## Structure de donnees proposee pour le stockage

Plutot que de coder les valeurs de performance en dur dans le moteur physique, stocker les donnees de reference dans un fichier structure (JSON ou YAML) versionne avec le code, ou chaque valeur porte sa source et ses conditions. Exemple de structure :

```json
{
  "aircraft": "SE 3130 / SE 313B",
  "engine": "Turbomeca Artouste IIC6",
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

Champs recommandes pour chaque entree :
- `quantity` : grandeur normalisee (utiliser un nom stable, pas une chaine libre, pour permettre l'agregation programmatique).
- `value` et `unit` : toujours en unites SI dans le fichier de stockage ; les conversions d'affichage (km/h, ft/min) se font a la lecture, jamais en dur dans les donnees sources.
- `conditions` : masse, altitude, vitesse, variante moteur. Une valeur sans conditions precisees n'est pas comparable a une autre.
- `source` : reference bibliographique complete, pas juste un nom de site.
- `authority_level` : 1 (certification officielle) a 4 (reference generique non specifique a l'appareil), pour ponderer les sources en cas de conflit.

## Methode d'analyse et de validation du moteur physique

1. **Ne jamais valider contre une valeur unique.** Pour chaque grandeur (VNE, Vmax, vario, plafond), calculer la fourchette [min, max] issue des sources de niveau 1 a 3, et considerer le modele comme coherent si sa sortie tombe dans cette fourchette, pas seulement s'il colle a une valeur precise.
2. **Ponderer par niveau d'autorite en cas de conflit reel.** Si deux sources de meme niveau divergent significativement (cas du vario Jane's vs ALAT/ESAM), ne pas trancher arbitrairement : documenter les deux valeurs, et si une troisieme source (idealement de niveau 1) est trouvee, l'utiliser comme arbitre.
3. **Journaliser chaque comparaison avec ses conditions.** Un test de vario a 1 332 m d'altitude (terrain Ossau) ne se compare pas directement a une reference donnee au niveau de la mer sans correction d'altitude densite. Le protocole de test doit soit reproduire les conditions de la reference, soit appliquer une correction documentee avant comparaison.
4. **Prioriser la recherche de sources de niveau 1 manquantes.** La Fiche de navigabilite DGAC n°24 et le manuel de vol constructeur sont les deux documents qui trancheraient la plupart des divergences actuelles ; leur obtention (contact Musee de l'Air et de l'Espace, Musee de l'ALAT, ou service historique de l'armee de Terre) est la piste la plus rentable pour lever l'incertitude, plus que l'ajustement fin de coefficients sur la base de sources de niveau 3 contradictoires.
5. **Ne pas reconcilier silencieusement.** Si le moteur physique doit choisir une valeur unique pour fonctionner (par exemple pour plafonner la vitesse), documenter explicitement dans le code et dans un commentaire quelle source a ete retenue et pourquoi, plutot que de moyenner arbitrairement deux chiffres d'origines differentes.

## Pistes pour completer la collecte

- Musee de l'Air et de l'Espace (Le Bourget) : fiche et archives constructeur SNCASE / Sud-Aviation.
- Musee de l'ALAT (Dax) : documentation operationnelle francaise, manuels de vol d'unites.
- Service Historique de la Defense : archives de certification et de programme.
- alat.fr et alat2.fr (association Christian Malcros) : autres volumes de la serie "Les Aeronefs de l'ALAT", potentiellement d'autres donnees techniques croisees.
- aircraft-reports.com : manuels de maintenance et reparation structurelle numerises, payants, utiles pour les donnees structurelles plutot que les performances en vol.
