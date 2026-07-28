#version 410 core

/*
 * zombie_eyes.vert
 * Lueur des yeux d'un zombie : billboard face caméra, même construction que
 * projectile.vert (axes droite/haut relus dans u_view), avec deux différences.
 * D'abord une couleur par instance, verte pour un marcheur et rouge pour un
 * largueur. Ensuite un grossissement avec la distance : un oeil de 9 cm serait
 * plus fin qu'un pixel depuis l'hélicoptère, alors que ces lueurs servent
 * justement à repérer la horde de loin. La taille apparente est donc bornée par
 * le bas (angle minimal), et le grossissement plafonne pour que la lueur reste
 * une lueur et ne devienne pas une tache.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec2 a_corner;   /* coin du quad, x/y dans [-0.5, 0.5] */
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_instance; /* xyz centre monde, w rayon de près (m) */
layout(location = 3) in vec3 a_color;    /* couleur et intensité de la lueur */

uniform mat4 u_model;  /* recalage repère-caméra (translation -origine de rendu) */
uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec3 u_camPos; /* position de la caméra (repère recalé) */

/* Grossissement avec la distance. Il faut un compromis : sans lui, un oeil de
   3 cm passe sous le pixel dès quelques dizaines de mètres et la horde
   lointaine devient invisible ; mais viser une taille apparente CONSTANTE (le
   rayon croissant proportionnellement à la distance, première version) donnait
   des lueurs d'un mètre de large à moyenne portée -- bien plus grosses que la
   tête qui les porte, donc des boules vertes flottantes plutôt que des yeux.

   D'où une croissance en racine de la distance : la taille apparente diminue
   toujours quand on s'éloigne (comme n'importe quel objet), mais moins vite que
   la perspective, ce qui garde un point lisible au loin. Au-delà du plafond, le
   rayon ne bouge plus et la lueur rétrécit normalement.

   Le réglage a été repris quand les yeux se sont avérés trop gros de près : le
   rayon de base est passé de 13 à 3,2 cm (voir app::ZombieHorde, EYE_RADIUS_M),
   ce qui aurait effacé la horde lointaine à croissance inchangée. La référence
   descend donc à 1 m et le plafond monte à 15, de sorte que le loin bouge peu
   (32 cm de rayon à 100 m, contre 46 avant) alors que le près est divisé par
   quatre (3,2 cm au contact, contre 13). */
const float GROWTH_REF_M = 1.0;   /* en deçà : rayon inchangé (au contact) */
const float MAX_GROWTH   = 15.0;  /* plafond, atteint vers 225 m */

out vec2 v_uv;
out vec3 v_color;

void main() {
    vec3 center = (u_model * vec4(a_instance.xyz, 1.0)).xyz;

    vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 up    = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);

    float dist   = length(u_camPos - center);
    float growth = clamp(sqrt(dist / GROWTH_REF_M), 1.0, MAX_GROWTH);
    /* Le rayon de près (a_instance.w) porte déjà l'échelle du zombie : un
       largueur garde donc sa lueur plus large à toute distance. */
    float radius = a_instance.w * growth;

    vec3 pos = center + (right * a_corner.x + up * a_corner.y) * radius * 2.0;

    v_uv        = a_uv;
    v_color     = a_color;
    gl_Position = u_proj * u_view * vec4(pos, 1.0);
}
