#version 410 core

/*
 * basic.frag
 * Éclairage diffus (modèle de Lambert) avec une seule source de lumière
 * directionnelle, plus un terme ambiant pour ne pas avoir d'ombres totalement noires.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3 v_normal;
in vec3 v_color;
in vec2 v_uv;

out vec4 frag_color;

uniform vec3  u_lightDir;  /* direction VERS la lumière, déjà normalisée */
uniform float u_alpha;     /* opacité du rendu : 1 = opaque (à poser à chaque usage) */
uniform vec3  u_tint;      /* teinte appliquée à la couleur des sommets (1,1,1 = aucune) */
uniform sampler2D u_texture;  /* lettrage en niveaux de gris : blanc = lettre */
uniform float u_texMix;    /* 0 = pas de lettrage (défaut), 1 = lettres pleines */
uniform float u_uvSpin;    /* décalage de u : fait tourner le lettrage sur la surface */

void main() {
    vec3  n       = normalize(v_normal);
    float diffuse = max(dot(n, normalize(u_lightDir)), 0.0);
    float ambient = 0.35;
    float light   = ambient + (1.0 - ambient) * diffuse;
    vec3  base    = v_color * u_tint;
    if (u_texMix > 0.0) {
        float lettre = texture(u_texture, vec2(v_uv.x + u_uvSpin, v_uv.y)).r * u_texMix;
        base         = mix(base, vec3(1.0), lettre);
    }
    frag_color    = vec4(base * light, u_alpha);
}
