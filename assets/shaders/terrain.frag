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
uniform vec3      u_seaColor;   /* couleur du plan de mer, pour fondre le bord */
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

    /* La mer du terrain est rendue dans la couleur unie du plan de mer (même
       éclairage), partout où l'altitude est ~ 0. Cela donne une surface d'eau
       homogène, sans l'écume ni les sillages blancs de la photo satellite, et un
       raccord invisible avec le plan de mer au bord du bloc.
       Mais sur une côte basse (bassin d'Arcachon), des terres habitées sont aussi
       sous 3 m : la seule condition d'altitude leur donnait un sol bleu-vert. On
       ajoute donc un test de couleur sur l'orthophoto. L'eau y a deux signatures :
       un rouge faible (canal R ~ 0.1-0.2, contre R ~ 0.45+ pour les villes et le
       sable) ET un bleu nettement supérieur au rouge. Cette dominante bleue évite de
       teinter les sols sombres mais neutres (ombres, toits foncés) qu'un test sur le
       seul rouge prenait pour de l'eau. Mêmes conditions que le filtre des bâtiments
       sur l'eau (voir render/Buildings.cpp), pour que sol et bâtiments s'accordent. */
    float lowAlt  = 1.0 - smoothstep(0.0, 3.0, v_worldPos.y);   /* 1 près du niveau 0 */
    float redLow  = 1.0 - smoothstep(0.22, 0.40, albedo.r);     /* 1 si rouge faible */
    float blueDom = smoothstep(0.04, 0.12, albedo.b - albedo.r);/* 1 si bleu domine (eau, pas une ombre) */
    float sea     = lowAlt * redLow * blueDom;                  /* eau = bas ET couleur d'eau */
    vec3  seaLit   = u_seaColor * min(light, 1.3);
    color          = mix(color, seaLit, sea);

    /* Brume : proportion croissante de couleur d'horizon avec la distance. */
    float dist = length(u_camPos - v_worldPos);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);

    frag_color = vec4(color, 1.0);
}
