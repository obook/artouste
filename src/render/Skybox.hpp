/*
 * Skybox.hpp
 * Ciel en dégradé : un grand triangle qui couvre tout l'écran et dont
 * la couleur varie selon la hauteur visée (plus clair vers l'horizon,
 * plus foncé vers le haut). Pas d'image de ciel, tout est calculé.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::render {

class Shader;

class Skybox {
public:
    Skybox();
    ~Skybox();

    Skybox(const Skybox&)            = delete;
    Skybox& operator=(const Skybox&) = delete;

    /* Dessine le ciel en arrière-plan, derrière tout le reste de la scène. */
    // j ai ajoute sun dir pour la pos du soleil
    void draw(Shader& shader, const mat4& invViewProj, const vec3& camPos, const vec3& sunDir) const;

private:
    unsigned int m_vao = 0;
};

}  /* namespace artouste::render */
