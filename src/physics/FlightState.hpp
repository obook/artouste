// État cinématique de l'appareil. En M2 (vol arcade) il est piloté en direct,
// sans forces ; à M3 il sera produit par l'intégration du modèle de vol.
// Repère monde Y-up. Angles en radians.

#pragma once

#include "util/Math.hpp"

namespace artouste::physics {

struct FlightState {
    vec3  position{0.0f, 0.0f, 0.0f};  // m, repère monde
    vec3  velocity{0.0f, 0.0f, 0.0f};  // m/s, repère monde
    float yaw   = 0.0f;                // lacet   (autour de Y)
    float pitch = 0.0f;                // tangage (autour de l'axe latéral)
    float roll  = 0.0f;                // roulis  (autour de l'axe avant)
};

}  // namespace artouste::physics
