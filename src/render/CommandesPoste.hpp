/*
 * CommandesPoste.hpp
 * Poses des commandes du poste 3dexport : manches, collectif et palonniers,
 * détachés de la cellule à la conversion (noeuds commande-*), tournent autour
 * d'un pivot mesuré sur le .glb. Les commandes sont conjuguées : celles du
 * copilote suivent celles du pilote. Mêmes angles que l'ancien poste
 * FlightGear.
 *
 * Repère racine de l'appareil : x vers la queue, y en haut, z à gauche du
 * pilote. Les commandes : cyclicLong > 0 manche en avant, cyclicLat > 0 à
 * droite, pedals > 0 pédale droite en avant, collective de 0 (en bas, pose
 * du modèle) à 1.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/BrasPilote.hpp"
#include "util/Math.hpp"

namespace artouste::render {

/* Pivots mesurés sur le .glb : haut de la bague du soufflet de chaque
   manche, bout arrière bas de la tige de chaque collectif (levier bas à
   gauche du siège), plancher sous chaque pédale. Droite et gauche vues de
   chaque siège. */
inline const vec3 PIVOT_MANCHE{-4.281f, -0.787f, -0.385f};
inline const vec3 PIVOT_MANCHE_COPILOTE{-4.281f, -0.787f, 0.370f};
inline const vec3 PIVOT_COLLECTIF{-3.800f, -0.807f, -0.056f};
inline const vec3 PIVOT_COLLECTIF_COPILOTE{-3.867f, -0.806f, 0.588f};
inline const vec3 PIVOT_PALONNIER_DROIT{-4.611f, -0.849f, -0.4805f};
inline const vec3 PIVOT_PALONNIER_GAUCHE{-4.611f, -0.849f, -0.2805f};
inline const vec3 PIVOT_PALONNIER_DROIT_COPILOTE{-4.611f, -0.849f, 0.2655f};
inline const vec3 PIVOT_PALONNIER_GAUCHE_COPILOTE{-4.611f, -0.849f, 0.4655f};

/* Bras du pilote au repos (épaule, coude, centre de la main), mesurés sur le
   .glb ; le convertisseur les découpe aux mêmes articulations (BRAS_PILOTE). */
inline const OsBras BRAS_PILOTE_GAUCHE{
    {-3.64f, -0.04f, -0.19f}, {-3.80f, -0.29f, -0.15f}, {-4.12f, -0.32f, -0.17f}};
inline const OsBras BRAS_PILOTE_DROIT{
    {-3.60f, -0.06f, -0.63f}, {-3.75f, -0.36f, -0.65f}, {-4.08f, -0.33f, -0.63f}};

/* Jambes du pilote au repos (hanche, genou, avant du pied), mesurées sur le
   .glb ; le convertisseur les découpe aux mêmes articulations. Presque
   tendues au repos (95 %) : elles se posent avec TENSION_JAMBE et la hanche
   ne bouge pas. */
inline const OsBras JAMBE_PILOTE_GAUCHE{
    {-3.76f, -0.52f, -0.25f}, {-4.12f, -0.47f, -0.25f}, {-4.55f, -0.70f, -0.20f}};
inline const OsBras JAMBE_PILOTE_DROITE{
    {-3.76f, -0.52f, -0.52f}, {-4.12f, -0.47f, -0.53f}, {-4.52f, -0.69f, -0.57f}};
inline constexpr float TENSION_JAMBE = 0.99f;

/* Appuis des pieds, pédales au neutre : l'avant du pied contre l'arrière de
   l'étrier, dans l'axe de sa pédale (le modèle met les pieds 8 à 9 cm à
   côté). Plus en avant, la jambe droite, plus courte dans le modèle, serait
   déjà tendue au neutre. */
inline const vec3 APPUI_PALONNIER_GAUCHE{-4.56f, -0.70f, -0.2805f};
inline const vec3 APPUI_PALONNIER_DROIT{-4.53f, -0.69f, -0.4805f};

/* Main gauche tournée sur l'axe de l'avant-bras (sens inverse des aiguilles
   d'une montre vu depuis le coude) pour donner l'illusion qu'elle saisit la
   poignée du collectif au lieu de s'y poser à plat. */
inline constexpr float MAIN_GAUCHE_ROTATION_DEG = 90.0f;

/* Haut des poignées en T des deux collectifs (tige allongée à la conversion,
   ALLONGE_COLLECTIF), mesuré sur le .glb. */
inline const vec3 POIGNEE_COLLECTIF{-4.147f, -0.449f, -0.060f};
inline const vec3 POIGNEE_COLLECTIF_COPILOTE{-4.234f, -0.493f, 0.608f};

/* Prises des mains, commandes au repos : centre de la main dans la barre en
   T du collectif (1,6 cm sous son sommet) et autour de la poignée du
   manche : les deux mains saisissent leur commande. */
inline const vec3 PRISE_COLLECTIF{-4.15f, -0.465f, -0.06f};
inline const vec3 PRISE_MANCHE{-4.131f, -0.247f, -0.39f};

/* Débattements à fond de commande, en degrés. */
inline constexpr float MANCHE_TANGAGE_DEG  = 15.0f;
inline constexpr float MANCHE_ROULIS_DEG   = 10.0f;
inline constexpr float COLLECTIF_LEVEE_DEG = 16.0f;
inline constexpr float PALONNIER_DEG       = 15.0f;

/* Rotation 'rotation' faite autour du point 'pivot'. */
inline mat4 autourDe(const vec3& pivot, const mat4& rotation) {
    return glm::translate(mat4(1.0f), pivot) * rotation * glm::translate(mat4(1.0f), -pivot);
}

/* Manche de pivot 'pivot' : tangage autour de l'axe latéral, puis roulis
   autour de l'axe avant-arrière. Le haut avance quand on pousse, part à
   droite (-z) quand on le penche à droite. */
inline mat4 poseManche(const vec3& pivot, float cyclicLong, float cyclicLat) {
    const float tangage = glm::radians(MANCHE_TANGAGE_DEG * glm::clamp(cyclicLong, -1.0f, 1.0f));
    const float roulis  = glm::radians(-MANCHE_ROULIS_DEG * glm::clamp(cyclicLat, -1.0f, 1.0f));
    return autourDe(pivot, glm::rotate(mat4(1.0f), tangage, vec3{0.0f, 0.0f, 1.0f}) *
                               glm::rotate(mat4(1.0f), roulis, vec3{1.0f, 0.0f, 0.0f}));
}

/* Collectif de pivot 'pivot' : la tige, inclinée vers l'avant, se relève
   autour de son bout arrière. */
inline mat4 poseCollectif(const vec3& pivot, float collective) {
    const float levee = glm::radians(-COLLECTIF_LEVEE_DEG * glm::clamp(collective, 0.0f, 1.0f));
    return autourDe(pivot, glm::rotate(mat4(1.0f), levee, vec3{0.0f, 0.0f, 1.0f}));
}

/* Triche de lisibilité, en vue cockpit seulement : de l'oeil du pilote, les
   collectifs sont sous le bord de l'image, et une vraie levée les ferait
   reculer et sortir du cadre. Les deux leviers y sont allongés dans leur axe,
   un peu au repos et davantage avec le pas : la main entre dans l'image
   quand on tire. Vue extérieure : poseCollectif, le vrai levier. */
inline constexpr float COLLECTIF_COCKPIT_ALLONGE = 1.2f;  /* au repos */
inline constexpr float COLLECTIF_COCKPIT_COURSE  = 0.15f;  /* en plus, à plein pas */

inline mat4 poseCollectifCockpit(const vec3& pivot, const vec3& poignee, float collective) {
    const float c   = glm::clamp(collective, 0.0f, 1.0f);
    const vec3  axe = glm::normalize(poignee - pivot);
    return poseCollectif(pivot, c) *
           autourDe(pivot, allongement(axe, COLLECTIF_COCKPIT_ALLONGE + COLLECTIF_COCKPIT_COURSE * c));
}

/* Pédale de pivot 'pivot' : elle bascule autour d'un axe latéral au
   plancher, la droite en avant et la gauche en arrière quand on pousse à
   droite. */
inline mat4 posePalonnier(const vec3& pivot, bool droite, float pedals) {
    const float sens  = droite ? 1.0f : -1.0f;
    const float angle = glm::radians(sens * PALONNIER_DEG * glm::clamp(pedals, -1.0f, 1.0f));
    return autourDe(pivot, glm::rotate(mat4(1.0f), angle, vec3{0.0f, 0.0f, 1.0f}));
}

}  /* namespace artouste::render */
