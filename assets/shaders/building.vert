#version 410 core

/*
 * building.vert
 * Shader de sommets des bâtiments : transformation MVP, normale dans le repère
 * monde, couleur du sommet et position monde (pour la brume au loin, comme le
 * terrain).
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
out vec3 v_worldPos;

void main() {
    vec4 world  = u_model * vec4(a_pos, 1.0);
    v_worldPos  = world.xyz;
    v_normal    = mat3(u_model) * a_normal;
    v_color     = a_color;
    gl_Position = u_proj * u_view * world;
}
