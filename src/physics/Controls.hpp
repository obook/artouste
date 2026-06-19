// Commandes pilote normalisées. Structure POD, sans dépendance
// au rendu ni aux entrées : c'est le contrat entre la couche input/ et physics/.

#pragma once

namespace artouste::physics {

struct Controls {
    float cyclicLateral      = 0.0f;  // [-1, +1]  manche latéral      -> roulis
    float cyclicLongitudinal = 0.0f;  // [-1, +1]  manche longitudinal -> tangage
    float collective         = 0.0f;  // [ 0, +1]  levier collectif    -> montée/descente
    float pedals             = 0.0f;  // [-1, +1]  palonniers          -> lacet
};

}  // namespace artouste::physics
