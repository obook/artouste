#version 410 core

/*
 * vegetation.vert
 * Arbres en billboards EN CROIX, instanciés, espèces tirées d'un atlas de sprites
 * (une colonne par espèce). Chaque arbre = DEUX quads verticaux perpendiculaires,
 * fixes dans le monde (orientés par l'azimut de l'instance) : il garde du volume
 * sous tous les angles, y compris vu du dessus.
 *
 * Fondu de densité à distance : loin de la caméra, on ne garde qu'une fraction des
 * arbres (les autres rétrécissent puis disparaissent), pour alléger le remplissage
 * au loin et éviter un tapis uniforme. Le tri se fait sur un rang par instance,
 * stable, donc un arbre grandit en douceur quand on s'en approche (pas de
 * scintillement).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

/* Par sommet (deux quads unitaires) : coin (x dans [-0.5,0.5], y dans [0,1]), UV,
   et plan (0 ou 1 = lequel des deux quads de la croix). */
layout(location = 0) in vec2 a_corner;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in float a_plane;
/* Par instance : centre au sol (monde), échelle (largeur m), espèce, azimut (rad). */
layout(location = 3) in vec3 a_center;
layout(location = 4) in float a_scale;
layout(location = 5) in float a_species;
layout(location = 6) in float a_azimuth;

uniform mat4  u_model;   /* recalage repère-caméra (comme le terrain / les bâtiments) */
uniform mat4  u_view;
uniform mat4  u_proj;
uniform vec3  u_camPos;   /* position caméra (repère recalé), pour le fondu de densité */
uniform float u_fogStart;
uniform float u_fogEnd;

/* Atlas : 4 colonnes (espèces). Cellule 256x512 -> billboard deux fois plus haut que
   large (l'arbre est calé sur la base de sa cellule). */
const float ATLAS_COUNT = 4.0;
const float ASPECT      = 2.0;
const float HALF_PI     = 1.5707963;
const float INV_TWO_PI  = 0.1591549;
/* Fraction d'arbres conservée à la distance de brume maximale (le reste s'efface). */
const float MIN_KEEP    = 0.18;

out vec2  v_uv;
out vec3  v_worldPos;
out float v_vfrac;   /* hauteur relative (0 base, 1 cime) : ombrage du pied */

void main() {
    vec3 center = (u_model * vec4(a_center, 1.0)).xyz;

    /* Fondu de densité : proportion d'arbres gardés selon la distance (1 de près,
       MIN_KEEP à la brume). Un arbre est gardé si son rang (stable, tiré de l'azimut)
       passe sous ce seuil ; près du seuil il rétrécit vers sa base au lieu de
       disparaître d'un coup. */
    float dist  = length(u_camPos - center);
    float dfade = smoothstep(u_fogStart * 0.6, u_fogEnd, dist);
    float keep  = mix(1.0, MIN_KEEP, dfade);
    float rank  = a_azimuth * INV_TWO_PI;              /* [0,1) */
    float grow  = clamp((keep - rank) / 0.12, 0.0, 1.0);
    if (grow <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);        /* hors clip : instance non dessinée */
        return;
    }

    int   sp     = int(a_species + 0.5);
    float width  = a_scale * grow;
    float height = a_scale * ASPECT * grow;

    /* Direction horizontale du plan : azimut de l'instance, +90 deg pour le second
       quad de la croix. Quad vertical (largeur le long de dir, hauteur selon Y). */
    float rot = a_azimuth + a_plane * HALF_PI;
    vec3  dir = vec3(cos(rot), 0.0, sin(rot));
    vec3  up  = vec3(0.0, 1.0, 0.0);
    vec3  pos = center + dir * (a_corner.x * width) + up * (a_corner.y * height);

    /* UV dans la colonne de l'espèce. */
    v_uv        = vec2((a_uv.x + float(sp)) / ATLAS_COUNT, a_uv.y);
    v_vfrac     = a_corner.y;
    v_worldPos  = pos;
    gl_Position = u_proj * u_view * vec4(pos, 1.0);
}
