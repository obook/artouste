/*
 * BuildingsEmprise.hpp
 * Emprises à ne pas extruder : hélipads et monuments 3D.
 *
 * Un bâtiment posé sur un pad le masque et provoque du z-fighting. Un bâtiment
 * sous un monument imbrique deux géométries : la tour Eiffel deviendrait un
 * bloc à fenêtres traversé par un treillis.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <vector>

namespace artouste::render {

class Terrain;

/* Zones dégagées de la carte, converties une fois en coordonnées monde. */
class Degagements {
public:
    explicit Degagements(const Terrain& terrain);

    /* Vrai si cette emprise gêne un hélipad : elle le contient, un de ses
       sommets en est tout près, ou son centre l'est. Le test
       point-dans-polygone attrape le grand bâtiment (un hôpital qui recouvre
       le pad), que la seule distance au centre ratait. */
    [[nodiscard]] bool surUnPad(const std::vector<float>& px,
                                const std::vector<float>& pz,
                                float                     cx,
                                float                     cz) const;

    /* Vrai si cette emprise tombe dans le rayon déclaré par un monument. */
    [[nodiscard]] bool sousUnMonument(const std::vector<float>& px,
                                      const std::vector<float>& pz,
                                      float                     cx,
                                      float                     cz) const;

private:
    struct Zone {
        float x       = 0.0f;
        float z       = 0.0f;
        float rayon2  = 0.0f;
    };
    std::vector<Zone> m_pads;
    std::vector<Zone> m_monuments;
};

} /* namespace artouste::render */
