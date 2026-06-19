/*
 * HelicopterModel.hpp
 * Hélicoptère simplifié, construit à partir de formes géométriques de base
 * (boîtes et pales). Il sert d'appareil de remplacement tant que le vrai modèle
 * 3D n'est pas chargé. Ses proportions s'inspirent de l'Alouette II réelle.
 * Les rotors tournent à vitesse fixe (animation purement visuelle).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include "render/Mesh.hpp"
#include "util/Math.hpp"

namespace artouste::render {

class Shader;

class HelicopterModel {
public:
    HelicopterModel();

    /* Dessine l'appareil. 'base' le place et l'oriente dans le monde ;
       'timeSeconds' fait tourner les rotors au fil du temps. */
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

}  /* namespace artouste::render */
