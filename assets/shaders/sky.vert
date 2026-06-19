#version 410 core

/*
 * sky.vert
 * Dessine un grand triangle qui couvre tout l'écran, sans tableau de sommets (VBO),
 * pour servir de support au ciel dégradé. Le fragment reconstruira le rayon de vue
 * à partir des coordonnées normalisées (NDC).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

out vec2 v_ndc;

void main() {
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_ndc       = pos * 2.0 - 1.0;
    gl_Position = vec4(v_ndc, 1.0, 1.0);  /* profondeur poussée sur le plan lointain */
}
