#version 410 core

/*
 * building.frag
 * Éclairage de Lambert (lumière directionnelle + ambiant) sur la couleur du
 * sommet, puis brume vers l'horizon identique à celle du terrain, pour que les
 * bâtiments lointains se fondent dans le ciel au lieu de surgir nettement.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3 v_normal;
in vec3 v_color;
in vec3 v_worldPos;

out vec4 frag_color;

uniform vec3  u_lightDir;   /* direction VERS la lumière, déjà normalisée */
uniform vec3  u_camPos;     /* position de la caméra (pour la distance de brume) */
uniform vec3  u_fogColor;   /* teinte de l'horizon vers laquelle on fond */
uniform float u_fogStart;   /* distance où la brume commence (m) */
uniform float u_fogEnd;     /* distance où tout est noyé dans la brume (m) */

void main() {
    vec3  n       = normalize(v_normal);
    float diffuse = max(dot(n, normalize(u_lightDir)), 0.0);
    float ambient = 0.4;
    float light   = ambient + (1.0 - ambient) * diffuse;
    vec3  color   = v_color * light;

    /* Brume : proportion croissante de couleur d'horizon avec la distance. */
    float dist = length(u_camPos - v_worldPos);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);

    frag_color = vec4(color, 1.0);
}
