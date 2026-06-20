#version 410 core

/*
 * sea.vert
 * Shader de sommets du plan de mer : transformation MVP et position monde
 * transmise au fragment pour calculer la brume au loin.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec3 a_pos;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_worldPos;

void main() {
    vec4 world  = u_model * vec4(a_pos, 1.0);
    v_worldPos  = world.xyz;
    gl_Position = u_proj * u_view * world;
}
