#version 410 core

/*
 * model.vert
 * Shader de sommets pour un modèle texturé : transformation MVP, normale dans
 * le repère monde, position monde et coordonnées de texture (UV) transmises au fragment.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;
layout(location = 3) in vec2 a_uv;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_normal;
out vec2 v_uv;
out vec3 v_worldPos;

void main() {
    vec4 world  = u_model * vec4(a_pos, 1.0);
    v_worldPos  = world.xyz;
    v_normal    = mat3(u_model) * a_normal;
    v_uv        = a_uv;
    gl_Position = u_proj * u_view * world;
}
