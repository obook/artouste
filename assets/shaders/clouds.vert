#version 410 core

/*
 * clouds.vert
 * Bouffées de nuage en billboards, instanciées. Chaque bouffée est un quad qui fait
 * entièrement face à la caméra (axes droite et haut de la matrice de vue), placé au
 * centre de l'instance et mis à l'échelle par sa taille. La hauteur relative dans le
 * nuage (a_vfrac) est transmise au fragment pour l'ombrage clair-en-haut / sombre-en-bas.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

/* Par sommet (quad unitaire) : coin (x,y dans [-0.5,0.5]) et UV. */
layout(location = 0) in vec2 a_corner;
layout(location = 1) in vec2 a_uv;
/* Par instance : centre (monde), taille (m), hauteur relative dans le nuage (0..1). */
layout(location = 2) in vec3 a_center;
layout(location = 3) in float a_size;
layout(location = 4) in float a_vfrac;

uniform mat4 u_model;  /* recalage repère-caméra (comme le terrain) */
uniform mat4 u_view;
uniform mat4 u_proj;

out vec2  v_uv;
out float v_vfrac;
out vec3  v_worldPos;

void main() {
    vec3 center = (u_model * vec4(a_center, 1.0)).xyz;

    /* Axes de la caméra (colonnes 0 et 1 de la vue, transposées) : billboard plein. */
    vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 up    = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
    vec3 pos   = center + right * (a_corner.x * a_size) + up * (a_corner.y * a_size);

    v_uv        = a_uv;
    v_vfrac     = a_vfrac;
    v_worldPos  = pos;
    gl_Position = u_proj * u_view * vec4(pos, 1.0);
}
