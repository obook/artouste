## v0.29.0 - 29 juillet 2026

### Nouvelles fonctionnalités

- **Recherche de mise à jour au lancement** (clé `verifier_maj`, active par défaut) : le numéro de la dernière version publiée est demandé dans un fil séparé, sans retarder ni la fenêtre ni le vol. S'il existe plus récent, le menu de démarrage l'annonce et propose d'ouvrir la page du projet, par le bouton `Télécharger` ou la touche `M`. Rien n'est envoyé au passage.
- **Nuit deux fois plus rapide que le jour** (clé `lune_vitesse`) : la vitesse du temps est multipliée entre le coucher et le lever. Avec les valeurs livrées, un cycle complet dure un quart d'heure, dix minutes de jour et cinq de nuit.
- **Configuration personnelle entretenue toute seule** : les options nouvelles sont ajoutées à `config.txt` avec leur documentation, les options renommées y sont migrées en gardant la valeur choisie, et un modèle effacé ou abîmé est réécrit depuis la copie embarquée dans l'exécutable.
- **Clés uniformisées en français** : `tree_max` devient `arbres_max`, `sun_time_scale` devient `soleil_vitesse`. Aucune action à faire, votre fichier est migré au premier lancement.
- **Mode zombie** : toucher le sol fend le réservoir au lieu d'entamer la vie. Un posé ferme coûte quelques litres, un crash vide les 575 L et cloue l'appareil au sol.

### Corrections

- **Panne sèche** : la turbine s'éteint dès que le réservoir est vide, quelle qu'en soit la cause. Elle pouvait auparavant tourner indéfiniment à sec.
- **Démarrage refusé sur fond de réservoir** : sous deux litres, le démarreur ne lance plus une séquence d'une minute vouée à s'éteindre avant le régime de vol.
- **Voyant carburant** maintenu en rouge réservoir vide, alors qu'il s'éteignait avec le reste de la planche au moment précis où il devenait utile. La ligne du HUD passe de `BAS` à `PANNE`.
- **Mode assisté** : la touche `M` est résolue selon la disposition réelle du clavier. La virgule d'un clavier AZERTY ne bascule plus l'assistance, et les dispositions autres qu'US et AZERTY fonctionnent.
- **Mode zombie** : un contact au sol trop doux pour se voir ne fait plus de bruit.

### Documentation

- Matériel requis précisé partout : **un PC et une manette de jeu suffisent**, rien à installer ni à configurer. Le clavier dépanne mais n'est pas recommandé.
- Logo et attribution IGN ajoutés au README, aux crédits, à la notice PDF, à l'étude du terrain et à la page du projet.
- Le numéro de version s'inscrit désormais tout seul dans la page du projet à chaque release.
