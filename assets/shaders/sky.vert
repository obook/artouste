#version 410 core

// Triangle plein écran (sans VBO) pour le ciel en dégradé. On reconstruit le
// rayon de vue dans le fragment à partir des coordonnées NDC.

out vec2 v_ndc;

void main() {
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_ndc       = pos * 2.0 - 1.0;
    gl_Position = vec4(v_ndc, 1.0, 1.0);  // profondeur au plan lointain
}
