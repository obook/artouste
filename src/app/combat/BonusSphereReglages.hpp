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

#include <algorithm>
#include <cmath>

namespace artouste::app {

/* Zombies qu'une même explosion doit faucher pour PRÉTENDRE à une fusée (1 :
   mise au point ; 2 : seuil de jeu, le double kill). Le seuil atteint, la
   fusée n'est encore que possible : voir chanceFuseeBonus plus bas. */
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

/* Une explosion meurtrière ne lance plus la fusée à coup sûr : elle la tire au
   sort. La chance part de BONUS_CHANCE_MANCHE1 et décroît en exponentielle vers
   BONUS_CHANCE_PLANCHER (environ 70 % en manche 3, 55 % en manche 5, 35 % en
   manche 10), pour que le kérosène se raréfie à mesure que le jeu durcit. Un
   kill multiple multiplie cette chance sans jamais la garantir : la trousse de
   secours et la sphère noire restent des récompenses, pas des acquis. */
constexpr float BONUS_CHANCE_MANCHE1  = 0.95f;
constexpr float BONUS_CHANCE_PLANCHER = 0.25f;
constexpr float BONUS_CHANCE_DECROIS  = 0.22f;  /* par manche */
constexpr float BONUS_CHANCE_X_SANTE  = 1.5f;   /* à partir du double kill */
constexpr float BONUS_CHANCE_X_MORT   = 2.0f;   /* à partir du triple kill */

[[nodiscard]] inline float chanceFuseeBonus(int wave, int killCount) noexcept {
    const float manches = static_cast<float>(std::max(wave, 1) - 1);
    const float base    = BONUS_CHANCE_PLANCHER
                          + (BONUS_CHANCE_MANCHE1 - BONUS_CHANCE_PLANCHER)
                                * std::exp(-BONUS_CHANCE_DECROIS * manches);
    const float multi   = killCount >= BONUS_MORT_KILL_MIN    ? BONUS_CHANCE_X_MORT
                          : killCount >= BONUS_SANTE_KILL_MIN ? BONUS_CHANCE_X_SANTE
                                                              : 1.0f;
    return saturate(base * multi);
}

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

/* Kérosène qu'un coup parti laisse toujours dans le réservoir. Tirer ne doit
   jamais couper la turbine : les sphères bleues naissent des zombies abattus,
   donc un joueur à sec qui ne peut plus tirer ne peut plus se ravitailler non
   plus, et la partie se termine sans qu'il y puisse rien. Le canon devient donc
   gratuit sur les derniers litres. Ce qui reste décide encore de la fin de
   partie, mais c'est la consommation de vol qui tranche, pas le tir. */
constexpr float TIR_RESERVE_L     = 5.0f;

/* Sphère noire ramassée : les zombies n'explosent pas tous ensemble mais l'un
   après l'autre, du plus proche de l'appareil au plus lointain, à cet
   intervalle (s). Points comptés un par un, comme ceux que le largueur emporte
   avec lui. */
constexpr float BONUS_MORT_INTERVALLE_S = 0.14f;
constexpr int   BONUS_MORT_SCORE        = 25;

/* Vie rendue par une trousse de secours, en fraction de la vie maximale. */
constexpr float BONUS_SANTE_FRACTION = 0.20f;

} /* namespace artouste::app */
