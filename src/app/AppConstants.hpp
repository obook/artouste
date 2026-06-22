/*
 * AppConstants.hpp
 * Constantes de scène partagées par les différentes unités de compilation de
 * la classe Application (mise en place de la scène, boucle, rendu, capture).
 * Comme Application.cpp a été découpé en plusieurs fichiers .cpp qui se
 * partagent ces réglages, on les regroupe ici en variables "inline" (C++17) :
 * une seule définition, visible partout où ce fichier est inclus.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::app {

/*
 * Position de l'oeil du pilote (siège droit) dans le repère corps : avant (X),
 * haut (Y), droite (Z). Réglée pour cadrer la planche de bord et la verrière, et
 * pour laisser voir les avant-bras du pilote sur le manche en partie basse (le
 * torse et le haut des bras, eux, sont retirés du modèle en vue cockpit, voir
 * LoadedHelicopter). Posée sur le siège droit, un peu recentrée, et avancée pour
 * rapprocher la planche de bord.
 */
inline const vec3 COCKPIT_EYE{3.55f, 1.86f, 0.46f};

/*
 * Brume atmosphérique : le terrain et la mer se fondent dans cette teinte de
 * ciel entre FOG_START et FOG_END (distance à la caméra, en mètres). Elle donne
 * la profondeur et masque le bord du terrain comme le plan de coupe lointain.
 */
inline const vec3      FOG_COLOR{0.74f, 0.80f, 0.86f};
inline constexpr float FOG_START = 4000.0f;
inline constexpr float FOG_END   = 22000.0f;

/*
 * Plan de mer : couleur de l'océan (accordée à la mer recolorée de l'orthophoto)
 * et demi-côté en mètres. Il est au même niveau que la mer du bloc de terrain
 * (dessinée à y=0 par l'orthophoto), pour un raccord net au bord du terrain : sans
 * cela, un plan plus bas laissait une marche visible (jointure en escalier) le long
 * du bord. Pas de z-fighting malgré ce niveau commun car ce plan est dessiné sans
 * écrire dans le tampon de profondeur (voir renderScene), donc le terrain le
 * recouvre toujours ; il ne se voit qu'au-delà du bord du terrain, jusqu'à l'horizon.
 */
inline const vec3      SEA_COLOR{0.180f, 0.259f, 0.271f};  /* (46,66,69) : mer de l'orthophoto */
inline constexpr float SEA_LEVEL = 0.0f;
inline constexpr float SEA_HALF  = 100000.0f;

}  /* namespace artouste::app */
