#version 410 core

/*
 * basic.vert
 * Shader de sommets de base : applique la transformation MVP (modèle, vue,
 * projection) et calcule la normale dans le repère monde pour l'éclairage.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_normal;
out vec3 v_color;

void main() {
    /* On suppose des transformations sans cisaillement ni mise à l'échelle non
       uniforme : la partie 3x3 de la matrice modèle suffit alors pour la normale. */
    v_normal    = mat3(u_model) * a_normal;
    v_color     = a_color;
    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);
}
