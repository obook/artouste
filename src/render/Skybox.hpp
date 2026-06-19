// Ciel en dégradé : un simple triangle plein écran dont le fragment calcule la
// couleur selon l'élévation du rayon de vue. Pas de cubemap. Le profil core
// impose un VAO lié même pour un tracé sans attribut.

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

    // Dessine le ciel (profondeur en lecture seule, écriture désactivée).
    void draw(Shader& shader, const mat4& invViewProj, const vec3& camPos) const;

private:
    unsigned int m_vao = 0;
};

}  // namespace artouste::render
