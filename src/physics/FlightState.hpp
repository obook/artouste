/*
 * FlightState.hpp
 * Position, vitesse et orientation de l'appareil à un instant donné.
 * En mode arcade, ces valeurs sont imposées directement ; avec le modèle de vol,
 * elles résultent de l'intégration des forces. Repère monde Y vers le haut, angles en radians.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::physics {

struct FlightState {
    vec3  position{0.0f, 0.0f, 0.0f};  /* m, repère monde */
    vec3  velocity{0.0f, 0.0f, 0.0f};  /* m/s, repère monde */
    float yaw   = 0.0f;                /* lacet   (autour de Y) */
    float pitch = 0.0f;                /* tangage (autour de l'axe latéral) */
    float roll  = 0.0f;                /* roulis  (autour de l'axe avant) */
};

}  /* namespace artouste::physics */
