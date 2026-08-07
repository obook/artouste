#version 140
#extension GL_ARB_explicit_attrib_location : require

/*
 * projectile.vert
 * Boulette toxique en billboard face caméra (contrairement à zombie.vert, qui
 * anime un vrai maillage 3D) : un seul quad, orienté à chaque image vers la
 * caméra en reconstruisant les axes droite/haut à partir de u_view (colonnes
 * de la matrice, voir glm::lookAt), plutôt qu'un azimut fixe comme la
 * végétation.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec2 a_corner;   /* coin du quad, x/y dans [-0.5, 0.5] */
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_instance; /* xyz centre monde, w échelle (diamètre, m) */

uniform mat4 u_model;  /* recalage repère-caméra (translation -origine de rendu) */
uniform mat4 u_view;
uniform mat4 u_proj;

out vec2 v_uv;

void main() {
    vec3 center = (u_model * vec4(a_instance.xyz, 1.0)).xyz;
    /* Axes caméra en repère monde : colonnes 0 et 1 de la matrice de vue
       (glm::lookAt place droite/haut de la caméra sur ces colonnes). */
    vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 up    = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);

    vec3 pos = center + (right * a_corner.x + up * a_corner.y) * a_instance.w;
    v_uv        = a_uv;
    gl_Position = u_proj * u_view * vec4(pos, 1.0);
}
