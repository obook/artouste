#version 410 core

/*
 * basic.frag
 * Éclairage diffus (modèle de Lambert) avec une seule source de lumière
 * directionnelle, plus un terme ambiant pour ne pas avoir d'ombres totalement noires.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3 v_normal;
in vec3 v_color;

out vec4 frag_color;

uniform vec3 u_lightDir;  /* direction VERS la lumière, déjà normalisée */

void main() {
    vec3  n       = normalize(v_normal);
    float diffuse = max(dot(n, normalize(u_lightDir)), 0.0);
    float ambient = 0.35;
    float light   = ambient + (1.0 - ambient) * diffuse;
    frag_color    = vec4(v_color * light, 1.0);
}
