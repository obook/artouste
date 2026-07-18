#version 410 core

/*
 * projectile.frag
 * Forme procédurale (pas de texture) : un disque toxique dégradé du centre
 * vers le bord, découpé net (alpha-test) plutôt que mélangé -- cohérent avec
 * le reste du rendu (zombie.frag, vegetation.frag), pas de gestion d'état de
 * mélange à ajouter au rendu.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec2 v_uv;

out vec4 frag_color;

void main() {
    vec2  p = v_uv - vec2(0.5);
    float d = length(p) * 2.0;  /* 0 au centre, 1 au bord du quad */
    if (d > 1.0) {
        discard;
    }
    /* Boulette brun foncé demandée (#3c302c) : teinte de base au milieu, coeur
       un peu éclairci et bord assombri pour un volume de petite boule. */
    vec3 base   = vec3(0.235, 0.188, 0.173);  /* #3c302c */
    vec3 center = base * 1.45;                 /* coeur éclairé */
    vec3 edge   = base * 0.6;                  /* bord dans l'ombre */
    vec3 color  = mix(center, base, smoothstep(0.0, 0.5, d));
    color       = mix(color, edge, smoothstep(0.5, 1.0, d));
    frag_color  = vec4(color, 1.0);
}
