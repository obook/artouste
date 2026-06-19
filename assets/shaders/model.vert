#version 410 core

// Shader pour modèle texturé : transformation MVP, normale monde, UV.

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;
layout(location = 3) in vec2 a_uv;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_normal;
out vec2 v_uv;

void main() {
    v_normal    = mat3(u_model) * a_normal;
    v_uv        = a_uv;
    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);
}
