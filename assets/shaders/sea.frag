#version 410 core

/*
 * sea.frag
 * Plan de mer : couleur unie de l'océan qui se fond dans la brume de l'horizon,
 * exactement comme le terrain, pour que la mer et les terres lointaines
 * disparaissent ensemble dans la même teinte de ciel.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3 v_worldPos;

out vec4 frag_color;

uniform vec3  u_seaColor;
uniform vec3  u_lightDir;   /* direction VERS la lumière (même éclairage que le terrain) */
uniform vec3  u_camPos;     /* position de la caméra (pour la distance de brume) */
uniform vec3  u_fogColor;   /* teinte de l'horizon vers laquelle on fond */
uniform float u_fogStart;   /* distance où la brume commence (m) */
uniform float u_fogEnd;     /* distance où tout est noyé dans la brume (m) */

void main() {
    /* Même éclairage demi-Lambert que le terrain, pour une surface plate (normale
       vers le haut) : sinon la mer de l'orthophoto, éclairée, paraît plus claire que
       ce plan, et un raccord de luminosité apparaît au bord du terrain. */
    float diffuse = dot(vec3(0.0, 1.0, 0.0), normalize(u_lightDir)) * 0.5 + 0.5;
    float light   = 0.55 + 0.55 * diffuse;
    vec3  color   = u_seaColor * min(light, 1.3);

    float dist = length(u_camPos - v_worldPos);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);
    frag_color = vec4(color, 1.0);
}
