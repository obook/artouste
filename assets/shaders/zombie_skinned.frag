#version 410 core

/*
 * zombie_skinned.frag
 * Éclairage identique à zombie.frag (demi-Lambert + appoint caméra + brume),
 * séparé seulement parce que le shader de skinning a ses propres entrées de
 * sommet. Le fragment, lui, ne change pas.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3  v_normal;
in vec2  v_uv;
in vec3  v_worldPos;
in float v_hitFlash;
in float v_colorSeed;

out vec4 frag_color;

uniform sampler2D u_texture;
uniform vec3      u_lightDir;  /* direction VERS la lumière, déjà normalisée */
uniform vec3      u_camPos;    /* position de la caméra (repère recalé) */
uniform vec3      u_fogColor;
uniform float     u_fogStart;
uniform float     u_fogEnd;

/* Rotation de teinte autour de l'axe des gris (luminance), en radians. */
vec3 hueShift(vec3 col, float a) {
    const vec3 k = vec3(0.57735);
    float c = cos(a);
    float s = sin(a);
    return col * c + cross(k, col) * s + k * dot(k, col) * (1.0 - c);
}

void main() {
    vec4 albedo = texture(u_texture, v_uv);
    if (albedo.a < 0.5) {
        discard;  /* alpha test : bords nets (cheveux, découpes du modèle) */
    }

    /* Variété entre zombies : on décale la teinte selon la graine d'instance.
       La texture (rampe de couleurs) mélange peau et vêtements dans les mêmes
       plages de saturation -- impossible de les distinguer proprement par ce
       seul critère -- donc l'angle max reste faible pour que la peau ne
       vire jamais vers une teinte improbable (vert, bleu...), au prix d'une
       variété un peu plus discrète sur les vêtements saturés. */
    float maxc = max(albedo.r, max(albedo.g, albedo.b));
    float minc = min(albedo.r, min(albedo.g, albedo.b));
    float sat  = maxc - minc;
    float amt  = (v_colorSeed * 2.0 - 1.0) * 0.6 * smoothstep(0.06, 0.30, sat);
    albedo.rgb = clamp(hueShift(albedo.rgb, amt), 0.0, 1.0);

    vec3  n       = normalize(v_normal);
    float diffuse = dot(n, normalize(u_lightDir)) * 0.5 + 0.5;

    vec3  viewDir   = normalize(u_camPos - v_worldPos);
    float headlight = max(dot(n, viewDir), 0.0);

    float light = 0.40 + 0.45 * diffuse + 0.45 * headlight;
    vec3  color = albedo.rgb * min(light, 1.2);

    float dist = length(u_camPos - v_worldPos);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);

    color = mix(color, vec3(1.0), v_hitFlash * 0.85);

    frag_color = vec4(color, 1.0);
}
