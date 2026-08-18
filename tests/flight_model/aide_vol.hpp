/*
 * aide_vol.hpp
 * Outils communs aux tests du modèle de vol : pas de simulation, avance de la
 * physique sur une durée, mesure de l'inclinaison du rotor.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "pas_simulation.hpp"
#include "physics/FlightModel.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace essais_vol {

using essais::SIM_DT;

/* Fait tourner la physique pendant "seconds", commandes tenues. */
inline void advance(artouste::physics::FlightModel&    model,
                    const artouste::physics::Controls& controls,
                    float                              seconds) {
    const int steps = static_cast<int>(seconds / SIM_DT);
    for (int i = 0; i < steps; ++i) {
        model.update(controls, SIM_DT);
    }
}

/* Inclinaison de l'axe du rotor par rapport à la verticale, en radians. */
inline float tiltAngle(const artouste::physics::FlightModel& model) {
    const artouste::vec3 up = model.body().orientation * artouste::vec3{0.0f, 1.0f, 0.0f};
    return std::acos(artouste::clamp(up.y, -1.0f, 1.0f));
}

} /* namespace essais_vol */
