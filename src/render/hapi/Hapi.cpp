/*
 * Hapi.cpp
 * Calcul du secteur et de la couleur d'une balise HAPI. Voir Hapi.hpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/hapi/Hapi.hpp"

#include <glm/glm.hpp>

namespace artouste::render {

HapiSector hapiSector(const HapiUnit& hapi, float horizDistM, float vertDeltaM) noexcept {
    constexpr float ON_SLOPE_HALF_DEG      = 0.375f;  /* 22'30" de part et d'autre */
    constexpr float SLIGHTLY_LOW_WIDTH_DEG = 0.25f;   /* 15' de plus, en dessous */
    const float     approachDeg = glm::degrees(std::atan(hapi.slopePercent / 100.0f));
    const float     elevDeg     = (horizDistM > 1.0f)
                                     ? glm::degrees(std::atan2(vertDeltaM, horizDistM))
                                     : approachDeg;  /* à l'aplomb : angle indéfini, "sur la pente" */
    if (elevDeg > approachDeg + ON_SLOPE_HALF_DEG) {
        return HapiSector::TooHigh;
    }
    if (elevDeg > approachDeg - ON_SLOPE_HALF_DEG) {
        return HapiSector::OnSlope;
    }
    if (elevDeg > approachDeg - ON_SLOPE_HALF_DEG - SLIGHTLY_LOW_WIDTH_DEG) {
        return HapiSector::SlightlyLow;
    }
    return HapiSector::TooLow;
}

HapiGlow hapiGlow(HapiSector sector, float timeSeconds) noexcept {
    const bool blinkOn = hapiBlinkOn(timeSeconds);
    switch (sector) {
        case HapiSector::TooHigh:
            return {true, !blinkOn};    /* vert clignotant */
        case HapiSector::OnSlope:
            return {true, false};       /* vert fixe */
        case HapiSector::SlightlyLow:
            return {false, false};      /* rouge fixe */
        case HapiSector::TooLow:
        default:
            return {false, !blinkOn};   /* rouge clignotant */
    }
}

}  /* namespace artouste::render */
