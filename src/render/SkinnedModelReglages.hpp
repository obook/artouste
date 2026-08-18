/*
 * SkinnedModelReglages.hpp
 * Mesures de calage du pack de personnages : taille visée, tranche de crâne,
 * position des yeux.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <assimp/matrix4x4.h>

namespace artouste::render {

/* Hauteur cible (m) d'un zombie une fois recalé, comme pour le modèle statique
   (render::combat::Zombies) : le pack est exporté en centimètres, pivot au sol
   mais centré arbitrairement en X/Z. */
constexpr float TARGET_HEIGHT_M = 1.80f;

/* Calibration de l'ancrage des yeux (voir la passe dédiée du constructeur).
   HEAD_SLICE_M est la tranche haute où l'on cherche l'os de tête ; le reste
   place le regard par rapport au NEZ, c'est-à-dire au point le plus avancé du
   crâne. Se caler sur la boîte du crâne ne suffisait pas : elle fait 18 à 29 cm
   de large selon la variante (oreilles et cheveux compris), si bien qu'un
   demi-écart pris sur elle posait les lueurs sur les oreilles. L'écart entre
   pupilles, lui, ne dépend pas de la coiffure : 6,4 cm chez l'humain. */
constexpr float HEAD_SLICE_M     = 0.12f;
constexpr float EYE_SPACING_M    = 0.032f;  /* demi-écart entre les deux yeux */
constexpr float EYE_ABOVE_NOSE_M = 0.015f;  /* les yeux sont juste au-dessus du nez */
constexpr float EYE_NOSE_INSET_M = 0.015f;  /* et en retrait de sa pointe */
/* Bornes verticales, comptées sous le sommet du crâne. Le point le plus avancé
   d'une tête n'est pas toujours le nez : sur trois des neuf variantes du pack,
   c'est le front ou une mèche, et le regard remontait alors sur le front (défaut
   signalé en jeu). On retient donc la PLUS BASSE des deux estimations, celle
   tirée du nez et celle tirée de la taille de la tête, avant de borner par le
   bas : trop haut se voit tout de suite, un peu bas passe pour un regard. */
constexpr float EYE_BELOW_CROWN_MIN_M = 0.11f;
constexpr float EYE_BELOW_CROWN_MAX_M = 0.18f;

inline mat4 toGlm(const aiMatrix4x4& a) {
    /* Assimp range par lignes, GLM par colonnes : la conversion transpose. */
    return mat4(a.a1,
                a.b1,
                a.c1,
                a.d1, /* */
                a.a2,
                a.b2,
                a.c2,
                a.d2, /* */
                a.a3,
                a.b3,
                a.c3,
                a.d3, /* */
                a.a4,
                a.b4,
                a.c4,
                a.d4);
}

} /* namespace artouste::render */
