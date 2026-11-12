#version 410 core

/*
 * lampadaires.vert
 * Un point lumineux par lampadaire (voir render/Lampadaires). La taille à
 * l'écran décroît avec la distance mais garde un plancher : les rues
 * lointaines restent des guirlandes de points, comme en vrai. Un tirage par
 * position donne à chaque point sa teinte (sodium ou LED) et son intensité.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec4 a_pos;  /* xyz : position monde, avant recalage ; w : 0 rue, 1 feu d'aérodrome, 2 feu de pad */

uniform mat4  u_model;   /* recalage repère-caméra */
uniform mat4  u_view;
uniform mat4  u_proj;
uniform vec3  u_camPos;  /* caméra, repère recalé */
uniform float u_hauteurEcran; /* hauteur de la fenêtre en pixels */

out vec3  v_color;
out float v_fondu;

const float TAILLE_M    = 2.6;  /* diamètre apparent de près, en mètres */
const float TAILLE_MIN  = 3.0;  /* plancher à l'écran, en pixels */
const float TAILLE_MAX  = 18.0;
const vec3  SODIUM      = vec3(0.98, 0.63, 0.27);
const vec3  LED         = vec3(0.92, 0.95, 1.00);
/* Balisage : aérodrome blanc, périmètre d'hélistation vert (OACI, annexe 14 vol. II). */
const vec3  FEUX[2]     = vec3[2](vec3(1.00, 1.00, 0.95), vec3(0.25, 1.00, 0.40));

float alea(vec3 p) {
    uvec3 q = uvec3(ivec3(floor(p)));
    uint  h = q.x * 374761393u + q.y * 668265263u + q.z * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return float(h ^ (h >> 16u)) / 4294967296.0;
}

void main() {
    vec3  pos  = (u_model * vec4(a_pos.xyz, 1.0)).xyz;
    vec4  clip = u_proj * u_view * vec4(pos, 1.0);
    float dist = max(length(u_camPos - pos), 1.0);
    /* Taille en pixels d'un objet de TAILLE_M mètres à cette distance. */
    float px = TAILLE_M * u_hauteurEcran * u_proj[1][1] * 0.5 / dist;
    gl_PointSize = clamp(px, TAILLE_MIN, TAILLE_MAX);
    gl_Position  = clip;

    float t = alea(a_pos.xyz);
    v_color = mix(SODIUM, LED, step(0.85, t)) * (0.7 + 0.3 * alea(a_pos.xyz + 7.0));
    if (a_pos.w > 0.5) {
        v_color = FEUX[int(a_pos.w) - 1];
    }
    /* Les points lointains s'effacent dans la brume, un peu avant le terrain. */
    v_fondu = 1.0 - smoothstep(6000.0, 14000.0, dist);
}
