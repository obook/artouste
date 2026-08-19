/*
 * BonusSphereReglages.hpp
 * Réglages du bonus de kérosène du mode zombie : la fusée qu'une explosion
 * meurtrière lance, et le volume qu'elle laisse en l'air.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::app {

/* Zombies qu'une même explosion doit faucher pour lancer une fusée (1 : mise au
   point ; 2 : seuil de jeu, le double kill). */
constexpr int BONUS_SPHERE_KILL_MIN = 1;

/* Volume posé : rayon (m), hauteur du centre au-dessus du relief (m). Bleu pour
   le bidon de kérosène (kill simple), rouge pour la trousse de secours (double
   kill et plus). */
constexpr float BONUS_SPHERE_HALF_M = 5.0f;
constexpr float BONUS_SPHERE_AGL_M  = 50.0f;
constexpr vec3  BONUS_SPHERE_COLOR_CARBURANT{0.15f, 0.40f, 0.95f};
constexpr vec3  BONUS_SPHERE_COLOR_SANTE{0.90f, 0.15f, 0.15f};
constexpr vec3  BONUS_SPHERE_COLOR_MORT{0.04f, 0.04f, 0.05f};

/* Zombies qu'une même explosion doit faucher pour changer le contenu de la
   sphère : soin au double kill, hécatombe au triple. */
constexpr int BONUS_SANTE_KILL_MIN = 2;
constexpr int BONUS_MORT_KILL_MIN  = 3;

/* Chandelle, en trois temps : montée (s) jusqu'au sommet (m), retombée (s)
   jusqu'à l'altitude du volume, éclosion (s). La graine mesure
   BONUS_SPHERE_SEED_SCALE du volume fini. */
constexpr float BONUS_SPHERE_RISE_S     = 2.4f;
constexpr float BONUS_SPHERE_APEX_M     = 60.0f;
constexpr float BONUS_SPHERE_FALL_S     = 0.9f;
constexpr float BONUS_SPHERE_GROW_S     = 0.15f;
constexpr float BONUS_SPHERE_SEED_SCALE = 0.08f;

/* Durée de vie (s) et fondu de fin (s) : à demi transparent à 20 s, éteint à 30 s. */
constexpr float BONUS_SPHERE_LIFE_S = 30.0f;
constexpr float BONUS_SPHERE_FADE_S = 20.0f;

/* Fusée : tube noir (rayon et longueur, m), flamme (rayon m, battement Hz,
   couleur). La flamme ne brûle qu'à la montée. */
constexpr float BONUS_ROCKET_RADIUS_M = 0.10f;
constexpr float BONUS_ROCKET_LEN_M    = 0.55f;
constexpr float BONUS_ROCKET_FLAME_M  = 0.22f;
constexpr float BONUS_ROCKET_FLAME_HZ = 18.0f;
constexpr vec3  BONUS_ROCKET_FLAME_COLOR{1.0f, 0.55f, 0.12f};

/* Tours par seconde du lettrage sur la surface de la sphère. */
constexpr float BONUS_TEXTE_TOURS_S = 0.12f;

/* Marge de ramassage (m), invisible en jeu : un passage un peu à côté compte. */
constexpr float BONUS_SPHERE_PICKUP_MARGIN_M = 3.0f;

/* Kérosène (L) rendu par un volume traversé, et coûté par un coup parti. */
constexpr float BONUS_SPHERE_FUEL_L = 50.0f;
constexpr float SHOT_FUEL_L       = 2.0f;

/* Sphère noire ramassée : les zombies n'explosent pas tous ensemble mais l'un
   après l'autre, du plus proche de l'appareil au plus lointain, à cet
   intervalle (s). Points comptés un par un, comme ceux que le largueur emporte
   avec lui. */
constexpr float BONUS_MORT_INTERVALLE_S = 0.14f;
constexpr int   BONUS_MORT_SCORE        = 25;

/* Vie rendue par une trousse de secours, en fraction de la vie maximale. */
constexpr float BONUS_SANTE_FRACTION = 0.20f;

} /* namespace artouste::app */
