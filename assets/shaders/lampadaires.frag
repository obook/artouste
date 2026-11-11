#version 410 core

/*
 * lampadaires.frag
 * Disque lumineux doux sur le point (gl_PointCoord), additif. u_nuit vaut 0 le
 * jour et 1 la nuit : l'éclairage s'allume au crépuscule.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3  v_color;
in float v_fondu;

out vec4 frag_color;

uniform float u_nuit;

void main() {
    float d = length(gl_PointCoord - vec2(0.5)) * 2.0;
    if (d > 1.0) {
        discard;
    }
    float halo = pow(1.0 - d, 1.8);
    float core = smoothstep(0.5, 0.0, d);
    vec3  color = v_color * halo + vec3(1.0, 0.95, 0.85) * core * 0.5;
    frag_color = vec4(color * 1.3, (halo + core) * u_nuit * v_fondu);
}
