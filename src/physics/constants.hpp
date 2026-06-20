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
inline constexpr float ROTOR_ENGAGE_TIME  = 15.0f;  /* s : rotor de 0 à 100 % (montée en régime après le lâcher de frein) */
inline constexpr float TURBINE_STOP_TIME  = 30.0f;  /* s : extinction, turbine de 100 % à 0 */
inline constexpr float ROTOR_STOP_TIME    = 40.0f;  /* s : rotor de 100 % à 0 (forte inertie, plus lent que la turbine) */

/* --- Carburant ---------------------------------------------------------------- */
/* Réservoir et consommation de l'Alouette II (d'après PANEL.md : ~580 L, 110 kg/h
 * en croisière, 155 kg/h à pleine puissance, autonomie ~4 h). On raisonne en
 * litres (kérosène ~0,8 kg/L). La turbine consomme dès qu'elle tourne, davantage
 * quand le collectif demande de la puissance. Réservoir vide -> extinction. */
inline constexpr float FUEL_CAPACITY_L   = 575.0f;  /* contenance du réservoir (L) */
inline constexpr float FUEL_BURN_MIN_LPH = 112.0f;  /* turbine lancée, collectif au mini (L/h) */
inline constexpr float FUEL_BURN_MAX_LPH = 194.0f;  /* pleine puissance (L/h) */
inline constexpr float FUEL_LOW_L        = 15.0f;   /* seuil du voyant bas carburant (~4 gallons) */

/* --- Traînée quadratique selon l'axe (repère corps) -------------------------- */
/* La traînée freine le mouvement : Force = -k * v * |v| sur chaque axe.
 * Elle est plus forte verticalement qu'horizontalement (forme de l'appareil). */
inline constexpr float KDRAG_FWD    = 2.2f;     /* N/(m/s)^2  axe avant */
inline constexpr float KDRAG_VERT   = 5.0f;     /* N/(m/s)^2  axe vertical */
inline constexpr float KDRAG_LAT    = 3.2f;     /* N/(m/s)^2  axe latéral */

/* --- Autorité des commandes (couple obtenu à pleine commande) ---------------- */
inline constexpr float ROLL_CTRL    = 3000.0f;  /* N.m  cyclique latéral */
inline constexpr float PITCH_CTRL   = 3000.0f;  /* N.m  cyclique longitudinal */
inline constexpr float YAW_CTRL     = 3500.0f;  /* N.m  palonniers */

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

/* --- Garde-fous numériques --------------------------------------- */
/* Limites de sécurité pour éviter que le calcul ne s'emballe. */
inline constexpr float MAX_SPEED    = 120.0f;   /* vitesse maximale, en m/s */
inline constexpr float MAX_OMEGA    = 6.0f;     /* vitesse de rotation maximale, en rad/s */

}  /* namespace artouste::physics */
