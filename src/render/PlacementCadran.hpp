/*
 * PlacementCadran.hpp
 * Pose d'un instrument FlightGear sur un cadran de la planche 3dexport. Les
 * instruments FlightGear regardent vers +X, haut en +Y, centrés sur leur
 * origine ; la planche du FBX est inclinée. On tourne donc l'instrument pour
 * que son X suive la normale du cadran et que son haut reste vers le haut.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::render {

/* Matrice qui pose l'instrument : origine en 'centre' avancée de 'avance'
   mètres le long de 'normale', X local sur la normale, Y local sur la
   verticale ramenée dans le plan du cadran, échelle uniforme 'echelle'. */
inline mat4 placementCadran(const vec3& centre, const vec3& normale, float echelle,
                            float avance) {
    const vec3 x = glm::normalize(normale);
    const vec3 y = glm::normalize(vec3{0.0f, 1.0f, 0.0f} - x * x.y);
    const vec3 z = glm::cross(x, y);
    mat4 m(1.0f);
    m[0] = vec4(x * echelle, 0.0f);
    m[1] = vec4(y * echelle, 0.0f);
    m[2] = vec4(z * echelle, 0.0f);
    m[3] = vec4(centre + x * avance, 1.0f);
    return m;
}

}  /* namespace artouste::render */
