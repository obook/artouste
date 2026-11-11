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
uniform float u_time;       /* secondes écoulées : le clapot dérive */
uniform float u_isDay;      /* 1 en plein jour, 0 soleil couché (ApplicationRender.cpp) */
const float NUIT = 0.18;    /* part de l'éclairage qui reste la nuit (lune) */
uniform vec2  u_originXZ;   /* origine de rendu : reconstitue le monde absolu */

const vec3 COULEUR_SOLEIL = vec3(1.0, 0.95, 0.85);

/* Reflet du soleil sur l'eau, à la FlightGear : des vaguelettes procédurales
   (trois ondes qui dérivent avec le temps) inclinent la normale, et le soleil
   s'y renvoie en une traînée qui scintille. Le clapot s'efface avec la distance,
   sinon ses ondes d'un mètre crépitent de moiré au loin : il ne reste alors que
   la traînée lisse. Un peu de ciel se reflète aussi en incidence rasante
   (Fresnel), ce qui éclaircit l'eau vue de loin. Renvoie l'intensité du reflet
   et la part de ciel à mélanger. */
vec2 refletEau(vec2 q, vec3 versCam, vec3 versSoleil, float dist, float t) {
    float clapot = 0.10 * (1.0 - smoothstep(500.0, 4000.0, dist));
    /* Trois ondes par axe, désaxées et de périodes sans rapport simple, sinon
       le clapot se lit comme un quadrillage. */
    float ax = sin(q.x * 0.9 + t * 1.3) + 0.7 * sin(dot(q, vec2(0.53, 0.71)) * 2.1 - t * 1.1)
             + 0.5 * sin(dot(q, vec2(-0.61, 0.37)) * 3.7 + t * 2.0);
    float az = sin(q.y * 1.1 - t * 1.0) + 0.7 * sin(dot(q, vec2(0.82, -0.44)) * 1.7 + t * 1.6)
             + 0.5 * sin(dot(q, vec2(0.29, 0.93)) * 4.1 - t * 1.4);
    vec3  n     = normalize(vec3(ax * clapot, 1.0, az * clapot));
    vec3  h     = normalize(versCam + versSoleil);
    float lobe  = mix(24.0, 90.0, 1.0 - smoothstep(500.0, 4000.0, dist));
    float spec  = pow(max(dot(n, h), 0.0), lobe);
    float fresnel = pow(1.0 - max(dot(n, versCam), 0.0), 4.0);
    /* Pas de reflet d'un soleil sous l'horizon. */
    float jour  = smoothstep(0.0, 0.15, versSoleil.y);
    return vec2(spec * jour * (0.7 + 1.0 * fresnel), 0.35 * fresnel);
}

void main() {
    /* Même éclairage demi-Lambert que le terrain, pour une surface plate (normale
       vers le haut) : sinon la mer de l'orthophoto, éclairée, paraît plus claire que
       ce plan, et un raccord de luminosité apparaît au bord du terrain. */
    float diffuse = dot(vec3(0.0, 1.0, 0.0), normalize(u_lightDir)) * 0.5 + 0.5;
    float light   = (0.55 + 0.55 * diffuse) * mix(NUIT, 1.0, u_isDay);
    vec3  color   = u_seaColor * min(light, 1.3);

    float dist = length(u_camPos - v_worldPos);
    /* Même fonction que dans terrain.frag : les deux surfaces d'eau se raccordent. */
    /* Clapot en monde absolu : v_worldPos est recalé sur la caméra à chaque
       image, les vagues suivraient l'appareil. */
    vec2 reflet = refletEau(v_worldPos.xz + u_originXZ, normalize(u_camPos - v_worldPos),
                            normalize(u_lightDir), dist, u_time);
    color = mix(color, u_fogColor, reflet.y) + COULEUR_SOLEIL * reflet.x;
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);
    frag_color = vec4(color, 1.0);
}
