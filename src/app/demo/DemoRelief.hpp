/*
 * DemoRelief.hpp
 * Survol du relief entre deux points de passage, et collectif de décollage.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/constants.hpp"
#include "util/Math.hpp"

#include <algorithm>
#include <cmath>

namespace artouste::app::demo_detail {

/* --- Relief intermédiaire (col, flanc de montagne) ---------------------------- */
inline constexpr float PAS_SONDE_RELIEF = 25.0f;  /* m : intervalle entre deux sondes sur la
                                                      route directe vers la cible. Fixe plutôt
                                                      que proportionnel à la distance : sur une
                                                      longue approche, un pas trop large sauterait
                                                      par-dessus une arête étroite. */
inline constexpr float MARGE_RELIEF     = 20.0f;  /* m : franchise visée au-dessus du point de
                                                      relief le plus haut trouvé sur la route. */
inline constexpr float PENTE_MONTEE_RELIEF = 0.25f;  /* pente de montée anticipée (m par m de
                                                      distance au point sondé), pour ne viser la
                                                      hauteur d'un relief qu'à mesure qu'il se
                                                      rapproche, pas dès qu'il entre dans
                                                      l'horizon sondé (jusqu'à dist, potentiellement
                                                      des centaines de mètres) -- plus raide que la
                                                      pente de descente (GAIN_ALT_RETOUR, 6 %) car
                                                      grimper n'est pas plafonné par le GPWS comme
                                                      la descente (collectifApprocheGpws). Sans
                                                      cette pente, un relief encore loin (donc pas
                                                      urgent) faisait grimper l'appareil aussitôt à
                                                      sa pleine hauteur, qu'il tenait alors sur tout
                                                      le reste du trajet jusqu'à lui : l'excédent à
                                                      perdre ensuite, une fois le relief dépassé,
                                                      pouvait dépasser ce que vitesseMinApproche
                                                      (filet de vitesse, tuné pour un excédent
                                                      modéré) suffit à écouler avant la finale,
                                                      d'où une descente quasi immobile sur 100 à
                                                      300 m (signalé le 16/07/2026). */

inline constexpr float VZ_DECOLLAGE      = 1.2f;  /* m/s : vitesse de montée visée au décollage (douce, contrôlée) */
inline constexpr float GAIN_VZ_DECOLLAGE = 0.06f; /* collectif par (m/s) d'écart à la vitesse de montée, au décollage */

/* Hauteur-sol minimale (même sens que hauteurCible : au-dessus du point courant) à
   tenir pour survoler avec MARGE_RELIEF le relief le plus haut rencontré entre
   position et cible, en anticipant chaque point sondé avec une pente de montée
   PENTE_MONTEE_RELIEF plutôt qu'en visant sa hauteur immédiatement. hauteurCible
   normale (GAIN_ALT_RETOUR * dist) suppose une pente de terrain régulière jusqu'au
   pad ; sur un col ou un flanc de montagne entre les deux, la ligne directe peut
   couper un point plus haut que le pad avant de l'atteindre. Sans anticipation, le
   pilote automatique ne réagit qu'à agl (la hauteur-sol sous l'appareil À L'INSTANT
   présent, voir collectifApprocheGpws) : le relief surgit alors trop tard pour que
   le collectif (lissé par rampeCollectif) ait le temps de faire grimper l'appareil,
   d'où un CFIT (vol contrôlé vers le terrain) plutôt qu'un problème de puissance.
   Recalculée à chaque appel depuis la position courante (horizon fuyant) : un
   relief déjà franchi sort naturellement de l'intervalle sondé, un relief encore à
   venir y reste tant qu'il n'a pas été dépassé.
   terrainHeight(x, z) renvoie l'altitude du sol à ce point (même fonction que
   render::Terrain::heightAt). Renvoie 0 si aucun relief intermédiaire n'impose de
   monter plus haut que hauteurCible ne le ferait déjà (cas courant, terrain plat ou
   en pente régulière jusqu'au pad). */
template <typename TerrainHeightFn>
inline float hauteurMinRelief(const vec3& position, const vec3& cible, float dist,
                              TerrainHeightFn&& terrainHeight) noexcept {
    if (dist <= PAS_SONDE_RELIEF) {
        return 0.0f;  /* trop près pour qu'un relief intermédiaire ait un sens */
    }
    const float solPosition = terrainHeight(position.x, position.z);
    float       hauteurMax  = 0.0f;
    for (float d = PAS_SONDE_RELIEF; d < dist; d += PAS_SONDE_RELIEF) {
        const float t   = d / dist;
        const float x   = position.x + (cible.x - position.x) * t;
        const float z   = position.z + (cible.z - position.z) * t;
        const float msl = terrainHeight(x, z) + MARGE_RELIEF;
        /* Hauteur-sol qu'il faut tenir MAINTENANT pour atteindre msl au point sondé
           en grimpant à PENTE_MONTEE_RELIEF -- pas la hauteur du relief elle-même :
           un point encore loin (d grand) n'exige donc pas de monter tout de suite. */
        const float hauteurSol = msl - solPosition - PENTE_MONTEE_RELIEF * d;
        if (hauteurSol > hauteurMax) {
            hauteurMax = hauteurSol;
        }
    }
    return hauteurMax > 0.0f ? hauteurMax : 0.0f;
}

/* Collectif de décollage : au lieu de tirer le levier à fond (cap à COLLECTIF_MAX) en
   visant l'altitude de survol, on asservit la VITESSE DE MONTÉE à une valeur douce
   (VZ_DECOLLAGE). Le collectif reste donc juste au-dessus de la sustentation et
   l'appareil quitte le pad lentement, sans à-coup. La rampe du levier (rampeCollectif)
   lisse encore l'instant initial. */
inline float collectifDecollage(float vitesseVerticale) noexcept {
    const float corr = GAIN_VZ_DECOLLAGE * (VZ_DECOLLAGE - vitesseVerticale);
    return saturate(physics::COLL_HOVER + corr);
}

}  /* namespace artouste::app::demo_detail */
