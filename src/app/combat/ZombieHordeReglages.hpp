/*
 * ZombieHordeReglages.hpp
 * Réglages de la horde, partagés par la simulation et le rendu.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::app {

/* Amplitude et fréquence d'un léger balancement vertical du corps, en plus de
   la marche animée par squelette (voir render::SkinnedZombies) : donne un peu
   de vie supplémentaire au déplacement, sans prétendre à un vrai cycle de
   marche (c'est l'animation du modèle qui s'en charge). */
constexpr float IDLE_BOB_AMPLITUDE_M = 0.05f;
constexpr float IDLE_BOB_FREQ_HZ     = 0.6f;
/* Durée de l'anim de chute avant despawn, une fois à 0 PV. */
constexpr float DEATH_ANIM_DURATION_S = 1.0f;
/* Durée du flash de coup touché (décroissance linéaire vers 0). */
constexpr float HIT_FLASH_DURATION_S = 0.15f;

/* Vitesse de marche de base (m/s), avant le facteur de difficulté des vagues
   tardives (voir WaveManager, étape 4). */
constexpr float WALK_SPEED_MS = 1.8f;
/* Portée maximale d'un jet (m, distance horizontale). Hors de portée, un
   zombie continue de marcher vers le joueur sans lancer. */
constexpr float TOXIC_RANGE_MAX_M = 60.0f;
/* Cooldown de jet par zombie (s), tiré aléatoirement dans cet intervalle à
   chaque jet pour désynchroniser la horde (éviter une salve groupée). */
constexpr float THROW_COOLDOWN_MIN_S = 2.0f;
constexpr float THROW_COOLDOWN_MAX_S = 4.0f;
/* Hauteur (m) à laquelle part le pneu, à peu près celle des mains d'un
   zombie qui lance le bras en avant. */
constexpr float THROW_ORIGIN_HEIGHT_M = 1.4f;

/* Rayon de base de la lueur (m), c'est-à-dire sa taille au contact. Le shader la
   fait ensuite grossir avec la distance pour qu'elle reste repérable depuis
   l'hélicoptère (voir zombie_eyes.vert) : cette valeur ne vaut donc que de tout
   près. 13 cm à l'origine, soit une boule de 26 cm sur une tête de 26 cm : les
   yeux mangeaient le visage dès qu'on approchait. Ramené à l'échelle d'un oeil
   qui luit, la croissance à distance étant relancée pour ne rien perdre de loin. */
constexpr float EYE_RADIUS_M = 0.032f;

/* Couleurs des lueurs, au-delà de 1 pour saturer franchement le rendu additif :
   vert pour un marcheur venu du bord de l'arène, rouge pour le largueur ET pour
   ce qu'il lâche. Le boss se repère ainsi de loin, avant même de distinguer sa
   silhouette (sa lueur est aussi trois fois plus large, à son échelle), et le
   rouge annonce du même coup quels marcheurs tomberont avec lui. */
inline const vec3 EYE_COLOR_WALKER{0.30f, 3.00f, 0.50f};
inline const vec3 EYE_COLOR_BROOD{3.20f, 0.18f, 0.10f};

} /* namespace artouste::app */
