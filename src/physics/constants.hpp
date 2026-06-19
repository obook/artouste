// Constantes physiques du modèle de vol (M3). Valeurs inspirées de l'Alouette II
// réelle mais ajustées pour un vol simple et stable :
// l'objectif est une dynamique reconnaissable, pas la haute fidélité.

#pragma once

namespace artouste::physics {

// --- Généralités -------------------------------------------------------------
inline constexpr float G            = 9.81f;    // m/s^2
inline constexpr float MASS         = 1100.0f;  // kg (en charge)

// Inerties par axe corps : X = avant (roulis), Y = haut (lacet), Z = droite
// (tangage). Ordre cohérent avec un vec3 (.x, .y, .z).
inline constexpr float I_ROLL       = 1200.0f;  // kg.m^2  autour de X
inline constexpr float I_YAW        = 3200.0f;  // kg.m^2  autour de Y
inline constexpr float I_PITCH      = 3500.0f;  // kg.m^2  autour de Z

// --- Poussée du rotor principal ----------------------------------------------
// Poussée le long de l'axe vertical du fuselage, proportionnelle au collectif.
// Calibrée pour que le vol stationnaire tombe à COLL_HOVER (poussée = poids).
inline constexpr float COLL_HOVER   = 0.55f;    // collectif de sustentation

// --- Traînée quadratique anisotrope (repère corps) ---------------------------
// Force = -k * v * |v| par axe. Plus forte en vertical qu'en horizontal.
inline constexpr float KDRAG_FWD    = 2.2f;     // N/(m/s)^2  axe avant
inline constexpr float KDRAG_VERT   = 5.0f;     // N/(m/s)^2  axe vertical
inline constexpr float KDRAG_LAT    = 3.2f;     // N/(m/s)^2  axe latéral

// --- Autorités de commande (couples à pleine commande) -----------------------
inline constexpr float ROLL_CTRL    = 3000.0f;  // N.m  cyclique latéral
inline constexpr float PITCH_CTRL   = 3000.0f;  // N.m  cyclique longitudinal
inline constexpr float YAW_CTRL     = 3500.0f;  // N.m  palonniers

// Couple réactif du rotor principal : nul au collectif de vol stationnaire,
// croît avec le collectif (le pilote corrige au palonnier).
inline constexpr float REACTIVE_TORQUE = 1500.0f;  // N.m à pleine variation

// --- Amortissements et stabilité ---------------------------------------------
inline constexpr float DAMP_ROLL    = 2500.0f;  // N.m/(rad/s)
inline constexpr float DAMP_PITCH   = 6000.0f;  // N.m/(rad/s)
inline constexpr float DAMP_YAW     = 4000.0f;  // N.m/(rad/s)

// --- Effets fins (M5) --------------------------------------------------------
// Effet de sol (IGE) : la poussée gagne jusqu'à GE_MAX au ras du sol, nul au-delà
// d'environ un diamètre rotor (coussin d'air sous le disque).
inline constexpr float GE_MAX     = 0.12f;   // +12 % au sol
inline constexpr float GE_HEIGHT  = 10.0f;   // m (~1 diamètre rotor)

// Effet de translation (ETL) : gain de portance avec la vitesse air horizontale,
// établi vers 25-30 kt.
inline constexpr float ETL_MAX    = 0.10f;   // +10 % établi
inline constexpr float ETL_V_LOW  = 7.0f;    // m/s (~14 kt) début du gain
inline constexpr float ETL_V_HIGH = 15.0f;   // m/s (~29 kt) plein effet

// Aide au pilotage : couple de rappel vers l'horizontale (stabilité augmentée).
// Sans elle l'appareil, sans rotor articulé modélisé, serait neutre en assiette
// et difficile à tenir. À réduire/retirer pour plus de réalisme plus tard.
inline constexpr float LEVEL_GAIN   = 6000.0f;  // N.m par unité de sin(inclinaison)

// --- Garde-fous numériques -----------------------------------------
inline constexpr float MAX_SPEED    = 120.0f;   // m/s
inline constexpr float MAX_OMEGA    = 6.0f;     // rad/s

}  // namespace artouste::physics
