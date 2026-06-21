#version 410 core

/*
 * shadow.vert
 * Shader de sommets de l'ombre portée : transformation MVP et transmission du
 * facteur de bord (0 au centre du disque, 1 sur son pourtour), porté par le canal
 * rouge de la couleur du sommet, qui sert au fragment à estomper l'opacité vers
 * le bord (contour flou).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec3 a_pos;
layout(location = 2) in vec3 a_color;  /* .r = facteur de bord (0 centre, 1 pourtour) */

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out float v_edge;

void main() {
    v_edge      = a_color.r;
    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);
}
