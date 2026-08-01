## v0.30.0 - 2 août 2026

### Nouvelles fonctionnalités

- **Monuments de Paris en volume** : Arc de Triomphe, Sacré-Coeur, Panthéon, Notre-Dame de Paris, Opéra Garnier et la Maison de la Radio posés sur la carte, avec rechargement automatique dès que `monuments.txt` change.
- **Reprise de vol à un endroit précis** : possibilité de relancer un vol depuis une position donnée plutôt qu'au pad de départ.
- **Souffle du rotor au ras du sol** : effet visuel du souffle du rotor sur le terrain proche.
- **Clairance de décollage variable** : la tour ne délivre plus toujours le même message radio au décollage.
- **Cycle jour/nuit démarré à 8h du matin** plutôt qu'à midi, hors temps réel.

### Corrections

- **Ressenti du vol adouci** : roulis et lacet moins vifs à pleine commande, mieux proportionnés à la masse de l'appareil ; le cyclique passe désormais par un léger retard physique (précession gyroscopique du rotor) avant de produire du couple.
- **Atterrissage automatique** : le roulis retrouve toute son autorité (il avait perdu plus de la moitié de son couple face au tangage), l'engagement se fait en douceur au lieu de faire sauter le manche, une interruption garde le collectif où il était plutôt que de sauter sur la position du levier physique, le message d'échec s'efface dès qu'un nouvel engagement réussit, et la vitesse d'approche plafonne désormais vers 70 kt au lieu de rester quasi en croisière jusqu'à 350 m du pad.
- **Carte de Paris** : position du pad de départ corrigée, balise HAPI manquante ajoutée, faces arrière des monuments qui restaient éclairées.
- **Mode assisté** : ne reste plus actif au décollage d'un vol lancé depuis le menu alors qu'il ne l'était pas au moment du lancement.
- **Erreurs GLFW au premier menu** : la salve de messages au lancement est désormais silencieuse.
- **Heure du HUD** : affiche le facteur d'accélération de la nuit, pas seulement celui du jour.

### Documentation

- Nombre de manettes reconnues précisé (2 242, via la base SDL_GameControllerDB) dans le README et sur la page du projet.
- Licence GPL v2 des modèles de monuments et leurs crédits consignés.
- Instructions de démarrage clarifiées dans le README et la page du projet.
