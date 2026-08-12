/*
 * constants.hpp
 * Constantes physiques du modèle de vol.
 * Les valeurs s'inspirent de l'Alouette II mais sont ajustées pour un vol
 * simple et stable : on cherche une dynamique reconnaissable, pas le réalisme absolu.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::physics {

/* --- Généralités ------------------------------------------------------------- */
inline constexpr float G            = 9.81f;    /* accélération de la pesanteur, en m/s^2 */
inline constexpr float MASS         = 1100.0f;  /* masse de l'appareil en charge, en kg */

/* Moments d'inertie : ils mesurent la résistance à la mise en rotation autour
 * de chaque axe du fuselage. X = avant (roulis), Y = haut (lacet), Z = droite
 * (tangage). Ordre cohérent avec un vec3 (.x, .y, .z). */
inline constexpr float I_ROLL       = 1200.0f;  /* kg.m^2  autour de X */
inline constexpr float I_YAW        = 3200.0f;  /* kg.m^2  autour de Y */
inline constexpr float I_PITCH      = 3500.0f;  /* kg.m^2  autour de Z */

/* --- Poussée du rotor principal ---------------------------------------------- */
/* Le rotor pousse le long de l'axe vertical du fuselage, proportionnellement au
 * collectif. La valeur est calée pour qu'au vol stationnaire la poussée équilibre
 * exactement le poids. */
inline constexpr float COLL_HOVER   = 0.55f;    /* collectif nécessaire pour se sustenter */

/* --- Pas collectif réel (graduation en degrés) -------------------------------- */
/* Le levier gradué en degrés de pas de pale, comme la machine réelle (manuel de
 * vol, planches FSHeli.ch, page 4) : plage utile 12 à 15 degrés, butée élastique
 * à 14 ou 14 deg 30 selon la phase, secours à 15 (franchissable sans pompage en
 * urgence). La correspondance est linéaire avec le collectif 0..1 :
 * pas = PAS_MIN + (PAS_MAX - PAS_MIN) x collectif. Elle n'est pas arbitraire :
 * avec 6..15 degrés, le collectif de sustentation (COLL_HOVER = 0,55) tombe à
 * 11,0 degrés, ce que donne aussi la loi de stationnaire de la page 8 du manuel
 * (12,8 degrés à 1500 kg au niveau de la mer ISA) ramenée à 1100 kg. */
inline constexpr float PAS_MIN_DEG            = 6.0f;   /* levier en butée basse */
inline constexpr float PAS_MAX_DEG            = 15.0f;  /* plein levier (secours) */
inline constexpr float PAS_BUTEE_HAUTE_DEG    = 14.5f;  /* butée élastique haute (14 deg 30) */

/* Le pas commande aussi ce que le rotor ABSORBE de la puissance turbine : à
 * 14 degrés (montée normale, butée élastique respectée) le rotor ne prend pas
 * tout ce que la turbine offre. C'est la lecture des planches Abb. 0-9 et 0-10
 * du manuel (la "Grenze der Blattverstellung" court sous les limites moteur),
 * et c'est ce qui réconcilie deux chiffres autrement incompatibles : la montée
 * de la planche Abb. 3-6 (8,10 m/s à 1100 kg, 90 km/h, pas 14) et le calage du
 * bilan à pleine puissance (voir POWER_ROTOR_W), qui donne 9,2 m/s dans les
 * mêmes conditions à plein levier. Fraction absorbée linéaire en pas, résolue
 * pour redonner exactement 8,10 m/s à 14 degrés : a(pas) = 1 - 0,0596 x
 * (15 - pas). Contrôle contre la planche numérisée, à 14 degrés et 90 km/h :
 *
 *     altitude-densité | modèle | planche
 *            0 m       |  8,10  |  8,10
 *         1000 m       |  6,91  |  7,05
 *         2000 m       |  5,82  |  6,00
 *         3000 m       |  4,81  |  4,95
 *         4000 m       |  3,88  |  3,90
 *
 * La même droite passe à 3 kW près par le point de stationnaire (11 degrés,
 * puissance de sustentation) : une seule pente porte donc le stationnaire, la
 * montée à 14 et le plein levier. Le plein collectif garde a = 1 : le palier
 * (Vmax 185 km/h) et les trois points de montée documentés à 1350, 1500 et
 * 1600 kg (calés à pleine puissance) ne bougent pas. */
inline constexpr float ABSORPTION_PENTE = 0.0596f;  /* fraction perdue par degré sous PAS_MAX_DEG */

/* --- Turbine Artouste --------------------------------------------------------- */
/* Démarrage en deux temps, comme une vraie turbine libre : la turbine (le
 * générateur) monte d'abord seule en régime, puis le rotor s'accouple et
 * s'accélère à son tour. Les pales ne tournent donc qu'une fois la turbine
 * lancée. L'arrêt est plus long : le rotor, lancé, met du temps à s'immobiliser. */
inline constexpr float TURBINE_START_TIME = 48.0f;  /* s : turbine de 0 à 100 %, calé sur le son turbine-start.wav (~48,6 s) */
inline constexpr float ROTOR_BRAKE_DELAY  = 3.0f;  /* s : turbine au régime, frein rotor encore serré, avant le lâcher */
inline constexpr float ROTOR_ENGAGE_TIME  = 35.0f;  /* s : rotor de 0 à 100 % (montée en régime après le lâcher de frein).
                                                       Le réel (embrayage centrifuge Artouste) impose 35-45 s ; on retient
                                                       35 s, bas de la fenêtre constructeur. */
inline constexpr float TURBINE_STOP_TIME  = 30.0f;  /* s : extinction, turbine de 100 % à 0 */
inline constexpr float ROTOR_STOP_TIME    = 40.0f;  /* s : rotor de 100 % à 0 (forte inertie, plus lent que la turbine) */

/* Démarrage rapide réservé au mode démo : seule la turbine monte en régime plus
 * vite que le réel, pour épargner au spectateur les 48 s de sifflement. La phase
 * rotor (frein puis embrayage centrifuge), spectaculaire, garde ses durées
 * réelles : ROTOR_BRAKE_DELAY et ROTOR_ENGAGE_TIME ci-dessus. */
inline constexpr float DEMO_TURBINE_START_TIME = 10.0f;  /* s : turbine 0 -> 100 % (accéléré) */

/* --- Température de la tuyère (gaz d'échappement, T4) -------------------------- */
/* La cible de température suit la relation pas/température du manuel (planche
 * Abb. 2-23, numérisée le 11/08/2026) : environ 27 degrés de t4 par degré de
 * pas, 1,6 degré par degré de température extérieure. En atmosphère normale
 * (l'air du simulateur), la température extérieure décroissant de 6,5 deg/1000 m,
 * la loi se réduit à : t4 = 407 + 27 x (pas - 12) - 10,4 x altitude(km).
 * La cible est bornée : 400 au plancher (turbine au régime, charge minimale,
 * repère PANEL.md) et 520 hors transitoire ; le régime transitoire (plancher de
 * puissance en altitude, voir POWER_FLAT_W) la pousse ensuite vers 550.
 * La tuyère rejoint sa cible avec une inertie de quelques secondes. */
inline constexpr float EXHAUST_TEMP_AMBIENT_C = 15.0f;   /* tuyère froide, turbine coupée */
inline constexpr float EXHAUST_TEMP_IDLE_C    = 400.0f;  /* turbine au régime, charge minimale */
inline constexpr float EXHAUST_TEMP_MAX_C     = 550.0f;  /* limite transitoire du manuel */
inline constexpr float EXHAUST_TEMP_TAU       = 4.0f;    /* s : inertie thermique (montée et descente) */

/* Refroidissement après coupure, en deux temps comme sur la machine. La flamme
   s'éteint d'un coup et les gaz cessent : la sonde décroche en quelques secondes
   (toujours EXHAUST_TEMP_TAU) vers la température du MÉTAL qui l'entoure, pas
   vers l'air ambiant. Ce métal, lui, met des minutes à rendre sa chaleur, et
   c'est cette longue queue que le pilote voit sur le cadran.
   EXHAUST_RESIDU : part de l'écart au-dessus de l'ambiante que garde le métal à
   l'instant de la coupure. Depuis 450 degrés, la chute franche s'arrête donc
   vers 250.
   EXHAUST_COOL_TAU : inertie de ce métal. Avec 240 s, il reste environ 85 degrés
   cinq minutes après la coupure et 35 au bout de dix, ce qui recoupe les ordres
   de grandeur d'une turbine d'hélicoptère. */
inline constexpr float EXHAUST_RESIDU         = 0.55f;
inline constexpr float EXHAUST_COOL_TAU       = 240.0f;  /* s : inertie de la chaleur résiduelle */
inline constexpr float EXHAUST_TEMP_WARN_C    = 480.0f;  /* voyant orange : surveiller (limite haute du vol normal) */
inline constexpr float EXHAUST_TEMP_MAXI_C    = 500.0f;  /* voyant rouge : limite continue franchie */
inline constexpr float T4_LOI_BASE_C          = 407.0f;  /* t4 à 12 degrés de pas, ISA niveau de la mer */
inline constexpr float T4_LOI_PAS_C           = 27.0f;   /* deg C par degré de pas */
inline constexpr float T4_LOI_ALT_C_PAR_KM    = 10.4f;   /* deg C perdus par km (1,6 x 6,5) */
inline constexpr float T4_LOI_PLAFOND_C       = 520.0f;  /* borne haute hors transitoire */
inline constexpr float T4_LOI_PAS_REF_DEG     = 12.0f;   /* pas de référence de la loi (celui de T4_LOI_BASE_C) */

/* --- Carburant ---------------------------------------------------------------- */
/* Réservoir et consommation de l'Alouette II (d'après PANEL.md : ~580 L, 110 kg/h
 * en croisière, 155 kg/h à pleine puissance, autonomie ~4 h). On raisonne en
 * litres (kérosène ~0,8 kg/L). La turbine consomme dès qu'elle tourne, davantage
 * quand le collectif demande de la puissance. Réservoir vide -> extinction. */
inline constexpr float FUEL_CAPACITY_L   = 575.0f;  /* contenance du réservoir (L) */
inline constexpr float FUEL_BURN_MIN_LPH = 112.0f;  /* turbine lancée, collectif au mini (L/h) */
inline constexpr float FUEL_BURN_MAX_LPH = 194.0f;  /* pleine puissance (L/h) */
inline constexpr float FUEL_LOW_L        = 15.0f;   /* seuil du voyant bas carburant (~4 gallons) */
/* Carburant minimal pour AMORCER un démarrage. La séquence dure une bonne minute
 * et brûle déjà près de deux litres avant que le rotor ne prenne son régime :
 * en dessous, la turbine s'éteindrait en cours de montée, après avoir fait tout
 * son bruit pour rien. La jauge affichant des litres entiers, "0 L" peut cacher
 * un demi-litre, assez pour lancer un démarrage voué à mourir : ce seuil ferme
 * la porte franchement plutôt que de laisser espérer. */
inline constexpr float FUEL_START_MIN_L  = 2.0f;    /* de quoi mener un démarrage à terme */
inline constexpr float FUEL_CAUTION_L    = 60.0f;   /* seuil de la LED jaune (~10 % du réservoir) */

/* --- Traînée quadratique selon l'axe (repère corps) -------------------------- */
/* La traînée freine le mouvement : Force = -k * v * |v| sur chaque axe.
 * Elle est plus forte verticalement qu'horizontalement (forme de l'appareil). */
/* KDRAG_FWD porte deux rôles à la fois, et c'est ce qui rend son calage délicat.
 * D'un côté il fixe l'assiette de croisière : en palier, la poussée doit s'incliner
 * juste assez pour vaincre la traînée, donc tan(assiette) = traînée / poids. De
 * l'autre il pèse dans la puissance parasite (traînée x vitesse), donc dans la
 * vitesse maximale en palier que donne le bilan de puissance plus bas.
 *
 * La valeur précédente (2,2) demandait 193 kW rien que pour traîner la cellule à
 * 160 km/h, davantage que la turbine entière : impossible, et responsable des
 * 20,5 degrés de piqué relevés en croisière au lieu des 8 à 10 attendus.
 *
 * Courbe calculée à 1100 kg et au niveau de la mer, les deux rôles étant liés, une
 * fois la puissance au rotor calée sur les performances documentées :
 *
 *     KDRAG_FWD | surface équiv. | assiette à 160 km/h | Vmax palier
 *        0,80   |    1,31 m2     |       8,3 deg       |  185,6 km/h
 *        0,87   |    1,42 m2     |       9,0 deg       |  180,9 km/h
 *        0,90   |    1,47 m2     |       9,3 deg       |  179,0 km/h
 *        1,00   |    1,63 m2     |      10,4 deg       |  173,2 km/h
 *
 * On retient 0,80 : l'assiette tombe dans la fenêtre demandée et la vitesse sur les
 * 185 km/h de Jane's. Une surface équivalente de 1,31 m2 est cohérente pour un
 * Alouette II, appareil franchement traînant (bulle exposée, poutre de queue en
 * treillis, patins fixes, moteur à l'air libre) ; elle encadre d'ailleurs la valeur
 * de 1,42 m2 que donne la résolution du bilan sur les seuls chiffres de l'ALAT. */
inline constexpr float KDRAG_FWD    = 0.8f;     /* N/(m/s)^2  axe avant */
inline constexpr float KDRAG_VERT   = 5.0f;     /* N/(m/s)^2  axe vertical */
inline constexpr float KDRAG_LAT    = 3.2f;     /* N/(m/s)^2  axe latéral */

/* --- Autorité des commandes (couple obtenu à pleine commande) ---------------- */
inline constexpr float ROLL_CTRL    = 1300.0f;  /* N.m  cyclique latéral */
inline constexpr float PITCH_CTRL   = 3000.0f;  /* N.m  cyclique longitudinal */
inline constexpr float YAW_CTRL     = 3500.0f;  /* N.m  palonniers */

/* Virage coordonné : avec de la vitesse, l'écoulement sur la dérive et le rotor de
 * queue tend à aligner le nez sur l'inclinaison donnée au cyclique latéral (comme
 * un avion qui vire en inclinant), plutôt que de seulement translater de côté.
 * Au stationnaire (pas de vitesse), rien ne s'ajoute : le cyclique latéral se
 * contente d'incliner l'appareil, qui part en crabe, pratique pour se décaler au
 * posé. Gain à confirmer en vol (retour pilote réel, contact du 07/08/2026) ;
 * la bascule progressive avec la vitesse réutilise ETL_V_LOW/ETL_V_HIGH ci-dessous. */
inline constexpr float TURN_COORD_GAIN = 1800.0f;  /* N.m à pleine inclinaison, vitesse établie */

/* --- Réponse du rotor principal (retard gyroscopique) ------------------------- */
/* Le plan des pales ne suit pas instantanément le manche : la précession
 * gyroscopique retarde sa bascule d'une constante de temps caractéristique du
 * rotor. Le rotor de queue, plus petit et beaucoup plus rapide, n'a pas cette
 * inertie : le retard ne s'applique donc qu'au cyclique (roulis, tangage), pas
 * au palonnier. */
inline constexpr float ROTOR_LAG_TAU = 0.15f;  /* s : constante de temps du plan de pales */

/* Le rotor principal exerce sur le fuselage un couple en sens inverse de sa
 * rotation. Sur l'Alouette II il tourne dans le sens horaire vu de dessus, donc ce
 * couple fait partir le nez vers la gauche. Nul au collectif de vol stationnaire, il
 * augmente avec le collectif ; le pilote le compense au palonnier droit. */
inline constexpr float REACTIVE_TORQUE = 1500.0f;  /* N.m à pleine variation */

/* --- Amortissements et stabilité --------------------------------------------- */
/* Ces couples s'opposent à la vitesse de rotation et calment les oscillations. */
inline constexpr float DAMP_ROLL    = 2500.0f;  /* N.m/(rad/s) */
inline constexpr float DAMP_PITCH   = 6000.0f;  /* N.m/(rad/s) */
inline constexpr float DAMP_YAW     = 4000.0f;  /* N.m/(rad/s) */

/* --- Effets aérodynamiques fins ---------------------------------------------- */
/* --- Bilan de puissance (montée et vitesse maximale en palier) ---------------- */
/* La turbine ne dispose que d'une puissance finie, et le vol la consomme de trois
 * façons : la puissance INDUITE (souffler de l'air vers le bas pour se sustenter),
 * la puissance de PROFIL (faire tourner les pales dans l'air) et la puissance
 * PARASITE (traîner la cellule à travers l'air). Ce qui reste, divisé par le poids,
 * donne le taux de montée disponible. Une seule équation borne ainsi la montée ET
 * la vitesse en palier : quand la puissance parasite mange tout l'excédent, monter
 * devient impossible, puis tenir l'altitude aussi, et l'appareil ne va pas plus
 * vite. La piquée, elle, tire son énergie de la pesanteur et n'est pas concernée :
 * c'est ce qui laisse la VNE atteignable en poussant sur le manche.
 *
 * Le calage précédent (une pénalité proportionnelle au seul taux de montée, calée
 * sur 4,2 m/s à plein collectif) était faux deux fois : 4,2 m/s est un chiffre à
 * 1600 kg et se lit à la puissance maximale CONTINUE, pas à plein collectif dans le
 * rouge. À la masse simulée de 1100 kg, les deux estimations indépendantes du
 * dossier (bilan de puissance recalé sur les points documentés à 1350, 1500 et
 * 1600 kg d'une part, pente masse/montée de heli-archive d'autre part) donnent
 * 7,7 à 8,3 m/s au niveau de la mer. Voir docs/technique/references-se3130.json. */
/* POWER_ROTOR_W n'est pas la puissance de la turbine : c'est ce qui reste au rotor
 * principal en montée continue, une fois payés le rotor de queue, la transmission,
 * les accessoires, et l'écart entre une puissance nominale de certificat et ce que
 * la machine tient réellement en continu. Elle vaut 74 % des 269 kW documentés.
 *
 * Elle n'est pas déduite de ces postes un à un, ce qui n'aurait donné qu'un
 * empilement d'hypothèses : elle est RÉSOLUE à partir des performances publiées.
 * C'est le seul degré de liberté restant une fois la puissance induite et la
 * puissance de profil calculées, et la valeur qui redonne les trois taux de montée
 * documentés est 200 kW. Contrôle, montée à VY au niveau de la mer :
 *
 *     masse   | modèle    | documenté
 *     1350 kg | 6,50 m/s  | 6,0 (heli-archive)
 *     1500 kg | 5,14 m/s  | 5,0 (heli-archive)
 *     1600 kg | 4,34 m/s  | 4,2 (Jane's) à 4,4 (ALAT)
 *
 * Le point à 1600 kg, le seul que deux sources indépendantes recoupent, tombe au
 * milieu de sa fourchette. À la masse simulée de 1100 kg, la même équation prédit
 * 9,4 m/s à VY et 4,7 m/s en montée verticale : c'est la prédiction du modèle, pas
 * un réglage, et elle remplace l'extrapolation linéaire plus grossière (7,7 à
 * 8,3 m/s) qui avait servi de cible avant que le bilan ne soit calé. */
inline constexpr float POWER_ROTOR_W      = 200000.0f; /* W : puissance disponible au rotor principal, niveau de la mer */

/* Régime de décollage (transitoire) : en altitude, la turbine tient un PLANCHER
 * de puissance au lieu de suivre la densité, tant que la température de tuyère
 * le permet (régulation à débit et t4, planches Abb. 0-9/0-10 : la limite passe
 * du débit carburant à la t4 vers 1800 m, et le certificat accorde 550 deg C en
 * transitoire). Le plancher ne change RIEN sous ~3000 m (200 kW x densité y est
 * plus grand) : palier, montée et calages existants sont intacts. Au-dessus, il
 * porte le plafond de stationnaire HES de ~3090 m (continu) à ~4070 m, la
 * valeur numérisée de la page 10 du manuel à 1100 kg en atmosphère normale
 * (+/-50 m).
 *
 * Calage : le plancher subit l'absorption comme la puissance continue (un seul
 * modèle, pas d'exception), donc 152,5 kW = besoin de stationnaire à 4070 m
 * (149,9 kW) divisé par l'absorption au pas de sustentation de cette altitude
 * (14,71 degrés, soit 0,983). Plafond mesuré sur le modèle, plein levier et à
 * convergence : 4078 m, atteint aussi bien par le haut que par le bas.
 *
 * Le plafond se prend PLEIN LEVIER, donc au-delà de la butée élastique, voyant
 * PAS jaune : c'est assumé, un plafond se tient à la puissance maximale. Ce qui
 * l'arrête n'est pas la puissance brute mais la fonte du plancher : au-delà de
 * 4070 m la surchauffe dépasse 1 et rogne le plancher, ce qui régule l'altitude
 * au lieu de la laisser filer. C'est ce qui rend le calage sûr malgré une
 * intersection RASANTE entre plancher et puissance de stationnaire (577 m de
 * plafond par kW vers le haut, 758 vers le bas, hors régulation) : mesuré,
 * passer de 152,5 à 156,5 kW ne déplace le plafond que de 4078 à 4090 m.
 *
 * ATTENTION quand même : toute retouche d'ABSORPTION_PENTE, de COLL_HOVER ou de
 * la correspondance levier/degrés (PAS_MIN_DEG, PAS_MAX_DEG) déplace ce plafond
 * de plusieurs centaines de mètres. Le revérifier après coup.
 *
 * La durée du régime n'est pas un chronomètre : sous le plafond il se tient
 * indéfiniment, l'alarme TMP à 550 degrés servant de marqueur au pilote ; la
 * fonte du plancher ne force la redescente qu'au-dessus du plafond. Voir
 * SURCHAUFFE_TAU_S et FlightModel. */
inline constexpr float POWER_FLAT_W       = 152500.0f; /* W : plancher transitoire tenu en altitude */
inline constexpr float SURCHAUFFE_TAU_S   = 240.0f;    /* s : inertie de la surchauffe (3 tau, soit ~12 min, pour l'établir) */
inline constexpr float SURCHAUFFE_PLEINE  = 0.11325f;  /* usage du plancher au plafond 4070 m : 1 - 200 kW x d(4070) / 152,5 kW */
inline constexpr float SURCHAUFFE_PAS_SEUIL_DEG = 13.5f; /* pas à partir duquel le transitoire est sollicité */
inline constexpr float SURCHAUFFE_PAS_PLAGE_DEG = 0.5f;  /* largeur de pas sur laquelle la sollicitation passe de 0 à 1 */
inline constexpr float FONTE_PLAGE              = 0.3f;  /* surchauffe au-delà de 1 qui annule complètement le plancher */

inline constexpr float POWER_INDUITE_W    = 91000.0f;  /* W : puissance induite au stationnaire, 1100 kg, niveau de la mer */
inline constexpr float POWER_PROFIL_W     = 58000.0f;  /* W : puissance de profil du rotor, niveau de la mer */
inline constexpr float V_INDUITE_HOVER    = 7.34f;     /* m/s : vitesse induite au stationnaire (poussée / 2 rho A) */
inline constexpr float V_PROFIL_REF       = 89.0f;     /* m/s : vitesse à laquelle la puissance de profil double */

/* La puissance induite s'effondre dès qu'on avance (le rotor brasse de l'air neuf
 * au lieu de recycler son propre souffle) : la vitesse induite passe de
 * V_INDUITE_HOVER au stationnaire à V_INDUITE_HOVER^2 / vitesse en croisière.
 * Approximation à une racine, exacte au stationnaire et à 1,5 % près à 160 km/h. */
inline constexpr float POWER_CLIMB_K = 20000.0f;  /* N par m/s de dépassement du taux de montée disponible :
                                                     raideur du plafond, pas un calage de performance. Assez
                                                     ferme pour que la montée s'arrête net à vyMax (dépassement
                                                     résiduel sous 0,5 m/s), assez souple pour rester stable
                                                     au pas de simulation. */

/* Effet de sol : près du sol, l'air repoussé par le rotor forme un coussin qui
 * augmente la poussée jusqu'à GE_MAX. L'effet disparaît au-delà d'environ un
 * diamètre de rotor. */
inline constexpr float GE_MAX     = 0.12f;   /* +12 % de poussée au ras du sol */
inline constexpr float GE_HEIGHT  = 10.0f;   /* m (~1 diamètre rotor) */

/* Effet de translation : en avançant, le rotor brasse de l'air neuf et gagne en
 * portance. Le gain s'établit progressivement vers 25-30 kt. */
inline constexpr float ETL_MAX    = 0.10f;   /* +10 % une fois établi */
inline constexpr float ETL_V_LOW  = 7.0f;    /* m/s (25 km/h au badin) début du gain */
inline constexpr float ETL_V_HIGH = 15.0f;   /* m/s (54 km/h au badin) plein effet */

/* Raffermissement aérodynamique avec la vitesse. Une fois la portance de
 * translation établie, le rotor travaille dans un air non perturbé et le
 * stabilisateur horizontal commence à mordre : l'appareil devient nettement plus
 * posé en tangage et en roulis, là où le stationnaire est neutre et demande une
 * correction permanente. On restitue cela en majorant les amortissements en
 * fonction du MÊME coefficient de translation que la poussée (pas de bascule
 * binaire "mode avion / mode latéral" : une seule grandeur continue pilote les
 * deux effets). Référence : FAA-H-8083-21B, chapitre 2 (translational lift). */
inline constexpr float STAB_AERO_GAIN = 0.5f;  /* +50 % d'amortissement à vitesse établie */

/* Aide au pilotage : un couple ramène doucement l'appareil à l'horizontale.
 * Comme on ne modélise pas le rotor articulé réel, l'appareil serait sinon neutre
 * en assiette et difficile à tenir.
 *
 * Ce rappel domine tout le reste : à pleine commande, le cyclique latéral pèse
 * ROLL_CTRL contre LEVEL_GAIN, ce qui plafonne l'inclinaison d'équilibre vers
 * 12 degrés. Appliqué tel quel à toute vitesse, il faisait du stationnaire une
 * plateforme aussi stable que la croisière, et rendait imperceptibles les effets
 * de régime ci-dessous (mesuré : 5 % d'écart de réponse entre les deux). On
 * l'atténue donc à basse vitesse jusqu'à LEVEL_HOVER_FRAC : le stationnaire
 * redevient un régime neutre, où l'appareil part et se rattrape au manche, tandis
 * que la croisière garde son maintien d'assiette. C'est la traduction directe de
 * la note technique : sans vitesse air, il n'y a pas d'écoulement structuré sur
 * le fuselage ni sur le stabilisateur, donc rien pour redresser l'appareil. */
inline constexpr float LEVEL_GAIN       = 6000.0f;  /* N.m par unité de sin(inclinaison) */
inline constexpr float LEVEL_HOVER_FRAC = 0.6f;     /* part du rappel conservée au stationnaire */

/* --- Mode assisté ------------------------------------------------------------ */
/* Couche de confort posée par-dessus les commandes du pilote (voir FlightAssist).
 * Elle ne change pas la physique : elle adoucit et corrige les entrées pour rendre
 * l'appareil tenable sans expérience. Valeurs à calibrer à l'usage. */
inline constexpr float ASSIST_ANTITORQUE_GAIN = 0.6f;   /* pousse le palonnier selon la variation de collectif */
inline constexpr float ASSIST_SMOOTH_TAU      = 0.18f;  /* s : lissage des inputs (anti sur-contrôle) */
inline constexpr float ASSIST_RECENTER_TAU    = 1.2f;   /* s : rappel doux du cyclique au neutre sans input */
inline constexpr float ASSIST_COLLECTIVE_RATE = 0.5f;   /* 1/s : variation max du collectif */
inline constexpr float ASSIST_TRANSITION_RATE = 2.0f;   /* 1/s : vitesse de bascule entre les modes (~0,5 s) */
inline constexpr float ASSIST_INPUT_DEADZONE  = 0.05f;  /* en-deçà, cyclique considéré relâché (rappel au neutre) */

/* --- Effets avancés ----------------------------------------------------------- */
/* Dégradation de la puissance avec l'altitude (densité de l'air). La densité
 * relative suit une exponentielle décroissante : rho/rho0 = exp(-altitude / H).
 * H est un compromis de jeu, pas l'atmosphère standard (il faudrait ~10 400 m) :
 * la pénalité est volontairement forcée pour que l'altitude compte sur la carte
 * des Pyrénées. À 1332 m (terrain Ossau) : densité 0,83, décollage vers 66 % de
 * collectif. Le mur de sustentation est à 4304 m, juste au-dessus du plafond de
 * stationnaire transitoire (4070 m, voir POWER_FLAT_W) : c'est désormais la
 * PUISSANCE qui borne la haute montagne, pas la portance, et les hauts sommets
 * restent accessibles mais exigeants (collectif de sustentation à 97 % vers
 * 4070 m), dans l'esprit de l'Alouette II, hélicoptère de haute montagne.
 *
 * Valeur passée de 5500 à 7200 le 11/08/2026 (décision utilisateur, alignement
 * sur le manuel de vol) : l'ancien mur à 3288 m tombait SOUS le plafond
 * transitoire du manuel et l'aurait rendu inatteignable. */
inline constexpr float AIR_DENSITY_SCALE  = 7200.0f;  /* m : hauteur caractéristique */

/* Le bilan de puissance, lui, se cale sur des performances documentées (vitesse
 * ascensionnelle et plafond du constructeur) : il doit donc raisonner sur
 * l'atmosphère RÉELLE, pas sur la densité durcie ci-dessus. Les deux notions
 * coexistent volontairement : AIR_DENSITY_SCALE durcit la SUSTENTATION pour rendre
 * la haute montagne exigeante (choix de jeu, mur de portance à 4304 m),
 * tandis que la montée et la vitesse en palier restent comparables aux fiches. Sans
 * cette séparation, la densité durcie écrasait aussi la puissance et le plafond
 * tombait vers 2500 m, très en dessous des 3200 m de l'ALAT, sur un appareil dont
 * la haute montagne est la raison d'être. */
inline constexpr float AIR_DENSITY_SCALE_REELLE = 10400.0f;  /* m : atmosphère standard */

/* VNE (vitesse à ne jamais dépasser) variable avec l'altitude, décroissante en
 * altitude (compressibilité sur les pales). Au-delà, une traînée d'onde
 * croissante freine l'appareil et matérialise la limite.
 *
 * NE PAS CONFONDRE avec la vitesse maximale en palier. Ce sont deux limites de
 * nature différente :
 *   - VNE 195 km/h : limite STRUCTURELLE du SE 3130, à ne franchir sous aucun
 *     prétexte. On ne l'atteint qu'en poussant sur le manche, jamais en palier.
 *     C'est elle que porte la bande rouge du cadran IAS (176-195 km/h, voir
 *     HudSuperOverlay.cpp) et que surveille le voyant de survitesse ;
 *   - 185 km/h : vitesse maximale EN PALIER, qui ne relève pas d'une limite mais
 *     de la puissance disponible face à la traînée. Elle sort donc du calcul, via
 *     KDRAG_FWD, et n'a pas à être écrite ici.
 * Les confondre faisait apparaître le décrochage de pale reculante en croisière
 * rapide au lieu de la seule approche de la limite structurelle. */
inline constexpr float VNE_SEA_LEVEL_MS   = 54.2f;    /* m/s (195 km/h au badin) au niveau de la mer */
inline constexpr float VNE_ALT_PLATEAU    = 1829.0f;  /* m : la VNE reste pleine jusque-là (6000 pieds au placard) */
inline constexpr float VNE_ALT_PENTE      = 0.00562f; /* (m/s) perdus par mètre au-dessus du plateau */
/* Traînée d'onde au-delà de la VNE. La valeur précédente (15) avait été calée
 * contre une traînée de cellule 2,2 fois trop forte, qui faisait déjà l'essentiel du
 * freinage ; avec KDRAG_FWD ramené à sa valeur physique, elle ne retenait plus rien
 * et une piquée plein manche filait à 224 km/h, trente au-dessus de la limite
 * structurelle. À 200, la même piquée plafonne à 205 km/h : la VNE reste
 * franchissable, comme il se doit puisque c'est le pilote qui doit s'en garder, mais
 * elle se paie tout de suite. */
inline constexpr float VNE_DRAG_K         = 200.0f;    /* N/(m/s)^2 : freinage au-delà de la VNE */

/* VNE à une altitude donnée (m/s) : pleine valeur jusqu'à VNE_ALT_PLATEAU, puis
 * décroissance linéaire. Partagée entre le modèle de vol (traînée d'onde) et le HUD
 * (LED de survitesse du cadran IAS), les deux devant suivre la table placardée.
 *
 * Forme et valeurs reprises de la table certifiée du TCDS FAA n°7H1 (rév. 16,
 * NOTE 2), à 2400 livres, soit 1089 kg : à onze kilos près la masse simulée, ce qui
 * fait de cette ligne la référence la mieux conditionnée du dossier. Elle donne un
 * PLATEAU à 105 noeuds jusqu'à 6000 pieds, puis 95 noeuds à 2743 m, 85 à 3658 m et
 * 80 à 4115 m ; la pente ci-dessus les restitue à moins d'un noeud près. Le modèle
 * précédent décroissait dès le niveau de la mer et se montrait donc beaucoup trop
 * sévère en montagne : au pic du Midi d'Ossau (2884 m) il donnait 164 km/h de VNE
 * contre 174 au placard. */
inline constexpr float vneAtAltitudeMs(float altitudeM) {
    if (altitudeM <= VNE_ALT_PLATEAU) {
        return VNE_SEA_LEVEL_MS;
    }
    const float vne = VNE_SEA_LEVEL_MS - VNE_ALT_PENTE * (altitudeM - VNE_ALT_PLATEAU);
    return vne < 20.0f ? 20.0f : vne;  /* garde-fou : la table s'arrête bien plus bas que le plafond du modèle */
}

/* Vol latéral ou arrière limité à 33 km/h, soit 18 kt du Flight Manual SE 3130,
 * rédigé en unités OTAN. Au-delà, le rotor anticouple sature et l'autorité au
 * palonnier diminue. Les instruments de bord, eux, sont en km/h : l'appareil est
 * de facture française d'époque, seule la version export affichait des noeuds. */
inline constexpr float SIDEWARD_V_MAX     = 9.3f;     /* m/s (33 km/h au badin) */

/* Vortex ring state : en descente verticale rapide et à faible vitesse, le rotor
 * retombe dans son propre souffle et perd de la portance. Danger maximal à
 * puissance partielle ; il disparaît dès qu'on reprend de la vitesse. */
inline constexpr float VRS_DESCENT_MIN    = 3.0f;     /* m/s : début du VRS */
inline constexpr float VRS_DESCENT_MAX    = 7.0f;     /* m/s : VRS développé */
inline constexpr float VRS_AIRSPEED_EXIT  = 7.0f;     /* m/s (25 km/h) : sortie par translation */
inline constexpr float VRS_THRUST_LOSS    = 0.35f;    /* fraction max de portance perdue */

/* Zone à éviter du diagramme hauteur-vitesse (indicateur HUD, voir FlightModel). */
inline constexpr float HV_AGL_MIN_M       = 3.0f;     /* m : sous cette hauteur-sol, pas d'indication */

/* Effet de flux transversal : pendant la transition vers le vol de translation,
 * l'écoulement induit chute à l'avant du disque rotor et augmente à l'arrière.
 * L'angle d'attaque monte donc à l'avant, et le rotor étant gyroscopique, la
 * réponse maximale apparaît 90 degrés plus loin dans le sens de rotation. C'est
 * un phénomène TRANSITOIRE, en cloche autour de 15 à 40 km/h, distinct de la
 * portance de translation même s'il vit dans la même plage de vitesse : il donne
 * les vibrations et le déséquilibre que le pilote contre au passage.
 *
 * SENS : le rotor de l'Alouette II tourne dans le sens HORAIRE vu de dessus
 * (convention française, voir REACTIVE_TORQUE). Quatre-vingt-dix degrés après
 * l'avant du disque dans ce sens, on tombe sur le côté DROIT : le disque se lève
 * à droite et l'appareil part en roulis à GAUCHE. La documentation américaine
 * (FAA-H-8083-21B ch. 2) annonce un roulis à droite parce que ses rotors tournent
 * en sens inverse ; ne pas recopier son sens tel quel. */
/* Valeur ramenée de 900 à 450 N.m le 12/08/2026. À 900, l'effet pesait 70 % de
 * l'autorité du cyclique latéral (ROLL_CTRL, 1300 N.m) et arrivait comme une
 * embardée que le rappel d'assiette rattrapait ensuite tout seul, ce qui ne
 * laissait rien à piloter. Le manuel FAA décrit une tendance au roulis que le
 * pilote contre d'une pointe de cyclique : à 450, soit 35 % de l'autorité, la
 * tendance reste franche et c'est au pilote de la tenir. */
inline constexpr float TRANSVERSE_ROLL    = 450.0f;   /* N.m au sommet de la cloche */
inline constexpr float TRANSVERSE_V_IN    = 4.0f;     /* m/s (14 km/h) : le phénomène s'installe */
inline constexpr float TRANSVERSE_V_PEAK  = 7.0f;     /* m/s (25 km/h) : plein effet */
inline constexpr float TRANSVERSE_V_OUT   = 12.0f;    /* m/s (43 km/h) : dissipé */

/* Décrochage de pale reculante : à l'approche de la VNE, la pale reculante voit
 * un vent relatif si faible qu'elle doit prendre un angle d'attaque énorme pour
 * porter, et finit par décrocher. Symptômes ressentis, dans cet ordre :
 * vibrations, cabrage, puis roulis vers le côté reculant. C'est CE mécanisme qui
 * limite en pratique la vitesse maximale ; il est calculé à part de la portance
 * de translation (le plafond de vitesse ne doit pas dépendre du gain d'ETL).
 *
 * SENS : rotor horaire vu de dessus, donc pale avançante à gauche et pale
 * RECULANTE À DROITE : le roulis part à droite. Miroir du cas américain, comme
 * pour le flux transversal ci-dessus.
 *
 * La traînée d'onde (VNE_DRAG_K) reste par-dessus : elle borne la vitesse, ces
 * couples-ci l'expliquent au pilote avant qu'il n'y arrive. */
inline constexpr float RBS_V_ONSET        = 0.96f;    /* fraction de la VNE (187 km/h au niveau de la mer) : premiers symptômes */
inline constexpr float RBS_V_FULL         = 1.05f;    /* fraction de la VNE (205 km/h) : décrochage franc */
inline constexpr float RBS_PITCH_UP       = 900.0f;   /* N.m : cabrage à plein décrochage */
inline constexpr float RBS_ROLL           = 700.0f;   /* N.m : roulis vers la pale reculante */

/* --- Alerte taux de descente (façon GPWS) -------------------------------------- */
/* Seuils de l'alerte "TAUX DE DESCENTE" du HUD (voir ui/HudAlarms.hpp et
   ApplicationHudInstruments.cpp) : plus on est bas, plus le taux de descente
   toléré est faible. Partagés avec le guidage du pilote automatique
   (DemoPilotDetail.hpp), qui vise un taux de descente sous ce seuil en approche
   pour ne jamais déclencher l'alerte -- au lieu de la couper pendant l'approche. */
inline constexpr float GPWS_MIN_AGL   = 2.0f;    /* m : alerte active seulement en vol (pas sur le pad) */
inline constexpr float GPWS_MAX_AGL   = 120.0f;  /* m : plafond de surveillance */
inline constexpr float GPWS_SINK_NEAR = 2.5f;    /* m/s : taux toléré près du sol */
inline constexpr float GPWS_SINK_FAR  = 8.0f;    /* m/s : taux toléré au plafond */

/* Taux de descente toléré par l'alerte GPWS à une hauteur-sol donnée (m/s) :
   enveloppe progressive entre GPWS_SINK_NEAR (au sol) et GPWS_SINK_FAR (au
   plafond de surveillance). */
inline constexpr float gpwsSinkLimitMs(float aglM) noexcept {
    const float f = (aglM < 0.0f) ? 0.0f : (aglM > GPWS_MAX_AGL) ? 1.0f : (aglM / GPWS_MAX_AGL);
    return GPWS_SINK_NEAR + (GPWS_SINK_FAR - GPWS_SINK_NEAR) * f;
}

/* --- Garde-fous numériques --------------------------------------- */
/* Limites de sécurité pour éviter que le calcul ne s'emballe. */
inline constexpr float MAX_SPEED    = 120.0f;   /* vitesse maximale, en m/s */
inline constexpr float MAX_OMEGA    = 6.0f;     /* vitesse de rotation maximale, en rad/s */

}  /* namespace artouste::physics */
