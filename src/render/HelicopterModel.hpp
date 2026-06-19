// Hélicoptère stylisé construit en primitives (placeholder M1, en attendant le
// modèle glTF Sketchfab). Proportions inspirées de l'Alouette II réelle :
// rotor principal tripale ~10,2 m de diamètre, rotor de queue ~1,9 m.
// Les rotors tournent à régime fixe ; la physique de vol viendra à M3.

#pragma once

#include "render/Mesh.hpp"
#include "util/Math.hpp"

namespace artouste::render {

class Shader;

class HelicopterModel {
public:
    HelicopterModel();

    // base : transformation monde de l'appareil (identité pour M1, statique).
    // timeSeconds : temps écoulé, sert à l'animation des rotors.
    void draw(Shader& shader, const mat4& base, float timeSeconds) const;

private:
    Mesh m_cabin;
    Mesh m_boom;
    Mesh m_mast;
    Mesh m_fin;
    Mesh m_skidLeft;
    Mesh m_skidRight;
    Mesh m_mainRotor;
    Mesh m_tailRotor;
};

}  // namespace artouste::render
