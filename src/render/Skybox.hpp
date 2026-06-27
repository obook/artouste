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

    /* Dessine le ciel en arrière-plan, derrière tout le reste de la scène.
       'invRotProj' est l'inverse de (projection * rotation caméra SEULE, sans la
       translation) : le fragment en tire directement la direction du rayon, sans
       soustraire la position caméra (qui, en milliers de mètres, faisait trembloter
       le soleil par perte de précision). 'sunDir' donne la position du soleil et
       'moonDir' celle de la lune (opposée au soleil), dessinée la nuit. */
    void draw(Shader& shader, const mat4& invRotProj, const vec3& sunDir,
              const vec3& moonDir) const;

private:
    unsigned int m_vao = 0;
};

}  /* namespace artouste::render */
