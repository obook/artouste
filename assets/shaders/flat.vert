#version 410 core

/*
 * flat.vert
 * Shader de sommets minimal : il ne fait que la transformation MVP. La couleur
 * est fixe (uniforme), ce qui sert par exemple à dessiner l'ombre portée.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec3 a_pos;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

void main() {
    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);
}
