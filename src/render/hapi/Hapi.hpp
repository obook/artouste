/*
 * Hapi.hpp
 * Sous-système HAPI (Helicopter Approach Path Indicator) : feu au sol à 4
 * secteurs verticaux (vert clignotant/fixe, rouge fixe/clignotant) qui indique
 * au pilote sa position par rapport à la pente d'approche visée. Extrait de
 * Terrain.hpp/Terrain.cpp, dont il reste dépendant (inclus par Terrain.hpp) :
 * réutilisé aussi par ApplicationGround.cpp (rendu 3D de la balise) et
 * ApplicationHud.cpp (repérage HUD).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include <cmath>
#include <string>

namespace artouste::render {

/* Balise HAPI : chaque terrain a ses propres balises, lues de hapi.txt dans
   son dossier (fichier absent : aucune). */
struct HapiUnit {
    std::string name;
    float       lon          = 0.0f;
    float       lat          = 0.0f;
    float       azimuthDeg   = 0.0f;  /* cap d'approche suivi par le pilote (0 = nord) */
    float       slopePercent = 6.0f;  /* pente d'approche visée (%), 6 % = valeur usuelle */
};

/* Rayon porteur d'une plate-forme d'hélisurface (m) : un peu plus grand que le
   disque dessiné (7 m), pour que les patins restent portés posé un peu décalé.
   Dans ce rayon, heightAt ne descend jamais sous le plateau du pad. */
inline constexpr float PAD_PLATFORM_RADIUS_M = 8.0f;

/* Hauteur du mât d'une balise HAPI au-dessus du sol (m) : le guide DGAC/STAC
   montre un petit trépied. Partagée entre le rendu 3D (ApplicationGround.cpp)
   et le repérage HUD (ApplicationHud.cpp), qui doivent viser le même point. */
inline constexpr float HAPI_MAST_M = 1.0f;

/* Période et fraction "allumée" du clignotement HAPI (s) : partagées pour que
   la lueur au sol et le point d'étiquette clignotent en phase. */
inline constexpr float HAPI_BLINK_PERIOD_S = 1.0f;
inline constexpr float HAPI_BLINK_ON_FRAC  = 0.5f;

/* Vrai pendant la phase "allumée" du clignotement HAPI à l'instant donné. */
[[nodiscard]] inline bool hapiBlinkOn(float timeSeconds) noexcept {
    return std::fmod(timeSeconds, HAPI_BLINK_PERIOD_S) < HAPI_BLINK_PERIOD_S * HAPI_BLINK_ON_FRAC;
}

/* Secteur HAPI vu depuis la balise en direction d'un point (distance horizontale
   et écart vertical par rapport à elle), comparé à la pente d'approche visée.
   Seuils repris du guide DGAC/STAC (§ 4.4) : 22'30" de part et d'autre de la
   pente pour "sur la pente", 15' de plus pour "légèrement trop bas". À l'aplomb
   de la balise (distance quasi nulle), l'angle n'est pas défini : on retombe sur
   "sur la pente" plutôt que d'afficher un secteur erratique. */
enum class HapiSector { TooHigh, OnSlope, SlightlyLow, TooLow };

[[nodiscard]] HapiSector hapiSector(const HapiUnit& hapi, float horizDistM,
                                    float vertDeltaM) noexcept;

/* Vert (sur/au-dessus de la pente) ou rouge (trop bas), et faux si ce secteur
   doit actuellement clignoter à l'arrêt (secteurs "trop haut"/"trop bas" en
   phase éteinte). Combine hapiSector() et hapiBlinkOn() pour les deux
   consommateurs (lueur 3D, points d'étiquette HUD). */
struct HapiGlow {
    bool green = true;
    bool off   = false;
};

[[nodiscard]] HapiGlow hapiGlow(HapiSector sector, float timeSeconds) noexcept;

}  /* namespace artouste::render */
