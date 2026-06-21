#version 410 core

/*
 * shadow.frag
 * Ombre portée douce : noir translucide dont l'opacité est pleine au centre puis
 * s'estompe jusqu'à zéro sur le pourtour (contour flou, pas de bord net). u_alpha
 * règle l'opacité au centre (plus elle est basse, plus l'ombre est claire).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in float v_edge;

out vec4 frag_color;

uniform float u_alpha;  /* opacité au centre du disque */

void main() {
    /* Plein jusqu'à 20 % du rayon, puis dégradé doux jusqu'au bord. */
    float a = u_alpha * (1.0 - smoothstep(0.2, 1.0, v_edge));
    frag_color = vec4(0.0, 0.0, 0.0, a);
}
