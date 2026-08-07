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
/* Modèle thermique simple : la température vise une cible déduite du régime
 * turbine (la tuyère chauffe quand la turbine monte en régime) et de la charge
 * collective (plus on tire de puissance, plus elle chauffe), qu'elle rejoint avec
 * une inertie. Repères du manuel (PANEL.md) : 400 à 480 degrés en vol normal,
 * 500 degrés en continu maxi, 550 degrés en transitoire. */
inline constexpr float EXHAUST_TEMP_AMBIENT_C = 15.0f;   /* tuyère froide, turbine coupée */
inline constexpr float EXHAUST_TEMP_IDLE_C    = 400.0f;  /* turbine au régime, charge minimale */
inline constexpr float EXHAUST_TEMP_MAX_C     = 550.0f;  /* plein collectif (limite transitoire du manuel) */
inline constexpr float EXHAUST_TEMP_TAU       = 4.0f;    /* s : inertie thermique (montée et descente) */
inline constexpr float EXHAUST_TEMP_WARN_C    = 480.0f;  /* voyant orange : surveiller (limite haute du vol normal) */
inline constexpr float EXHAUST_TEMP_MAXI_C    = 500.0f;  /* voyant rouge : limite continue franchie */

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
inline constexpr float KDRAG_FWD    = 2.2f;     /* N/(m/s)^2  axe avant */
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
/* Effet de sol : près du sol, l'air repoussé par le rotor forme un coussin qui
 * augmente la poussée jusqu'à GE_MAX. L'effet disparaît au-delà d'environ un
 * diamètre de rotor. */
inline constexpr float GE_MAX     = 0.12f;   /* +12 % de poussée au ras du sol */
inline constexpr float GE_HEIGHT  = 10.0f;   /* m (~1 diamètre rotor) */

/* Effet de translation : en avançant, le rotor brasse de l'air neuf et gagne en
 * portance. Le gain s'établit progressivement vers 25-30 kt. */
inline constexpr float ETL_MAX    = 0.10f;   /* +10 % une fois établi */
inline constexpr float ETL_V_LOW  = 7.0f;    /* m/s (~14 kt) début du gain */
inline constexpr float ETL_V_HIGH = 15.0f;   /* m/s (~29 kt) plein effet */

/* Aide au pilotage : un couple ramène doucement l'appareil à l'horizontale.
 * Comme on ne modélise pas le rotor articulé réel, l'appareil serait sinon neutre
 * en assiette et difficile à tenir. À réduire ou retirer pour plus de réalisme. */
inline constexpr float LEVEL_GAIN   = 6000.0f;  /* N.m par unité de sin(inclinaison) */

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
 * des Pyrénées. À 1332 m (terrain Ossau) : densité 0,78, décollage vers 63 % de
 * collectif. Le stationnaire devient impossible vers 3300 m, ce qui laisse les
 * hauts sommets accessibles mais exigeants, dans l'esprit de l'Alouette II,
 * hélicoptère de haute montagne. */
inline constexpr float AIR_DENSITY_SCALE  = 5500.0f;  /* m : hauteur caractéristique */

/* VNE (vitesse à ne jamais dépasser) variable avec l'altitude : 105 kt au sol,
 * décroissante en altitude (compressibilité sur les pales). Au-delà, une traînée
 * d'onde croissante freine l'appareil et matérialise la limite. */
inline constexpr float VNE_SEA_LEVEL_MS   = 54.0f;    /* m/s (105 kt) au niveau de la mer */
inline constexpr float VNE_ALT_GRADIENT   = 4511.0f;  /* m : altitude pour perdre 25 % de VNE */
inline constexpr float VNE_DRAG_K         = 15.0f;    /* N/(m/s)^2 : freinage au-delà de la VNE */

/* VNE à une altitude donnée (m/s) : pleine valeur au niveau de la mer, décroissance
 * linéaire jusqu'à -25 % à VNE_ALT_GRADIENT, constante au-delà. Partagée entre le
 * modèle de vol (traînée d'onde) et le HUD (LED de survitesse du cadran IAS). */
inline constexpr float vneAtAltitudeMs(float altitudeM) {
    const float f         = altitudeM / VNE_ALT_GRADIENT;
    const float altFactor = (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f;
    return VNE_SEA_LEVEL_MS * (1.0f - 0.25f * altFactor);
}

/* Vol latéral ou arrière limité à 18 kt (Flight Manual SE 3130). Au-delà, le
 * rotor anticouple sature et l'autorité au palonnier diminue. */
inline constexpr float SIDEWARD_V_MAX     = 9.3f;     /* m/s (18 kt) */

/* Vortex ring state : en descente verticale rapide et à faible vitesse, le rotor
 * retombe dans son propre souffle et perd de la portance. Danger maximal à
 * puissance partielle ; il disparaît dès qu'on reprend de la vitesse. */
inline constexpr float VRS_DESCENT_MIN    = 3.0f;     /* m/s : début du VRS */
inline constexpr float VRS_DESCENT_MAX    = 7.0f;     /* m/s : VRS développé */
inline constexpr float VRS_AIRSPEED_EXIT  = 7.0f;     /* m/s (~14 kt) : sortie par translation */
inline constexpr float VRS_THRUST_LOSS    = 0.35f;    /* fraction max de portance perdue */

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
