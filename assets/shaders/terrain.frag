#version 410 core

/*
 * terrain.frag
 * Drape l'orthophoto réelle sur le relief : couleur lue dans la texture,
 * éclairage de Lambert adouci (pour faire ressortir les pentes), puis brume
 * vers l'horizon qui fait fondre le lointain dans la couleur du ciel. La brume
 * masque aussi le bord du terrain et le plan de coupe lointain de la caméra.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_worldPos;

out vec4 frag_color;

uniform sampler2D u_texture;
uniform vec3      u_lightDir;   /* direction VERS la lumière, déjà normalisée */
uniform vec3      u_camPos;     /* position de la caméra (pour la distance de brume) */
uniform vec3      u_fogColor;   /* teinte de l'horizon vers laquelle on fond */
uniform float     u_fogStart;   /* distance où la brume commence (m) */
uniform float     u_fogEnd;     /* distance où tout est noyé dans la brume (m) */

void main() {
    vec3 albedo = texture(u_texture, v_uv).rgb;

    /* Demi-Lambert : la lumière sculpte le relief sans plonger les versants à
       l'ombre dans le noir total. */
    vec3  n       = normalize(v_normal);
    float diffuse = dot(n, normalize(u_lightDir)) * 0.5 + 0.5;
    float light   = 0.55 + 0.55 * diffuse;
    vec3  color   = albedo * min(light, 1.3);

    /* Brume : proportion croissante de couleur d'horizon avec la distance. */
    float dist = length(u_camPos - v_worldPos);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);

    frag_color = vec4(color, 1.0);
}
