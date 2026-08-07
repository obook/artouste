#version 140

/*
 * souffle.frag
 * Forme d'une bouffée de poussière, entièrement procédurale (pas de texture,
 * comme projectile.frag) : un disque au bord fondu, rongé par un bruit à deux
 * octaves décalé par la graine de l'instance. Deux bouffées voisines n'ont donc
 * jamais le même contour, ce qui suffit à faire un nuage crédible par simple
 * accumulation de billboards translucides.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec2  v_uv;
in float v_alpha;
in float v_graine;
in vec3  v_couleur;

out vec4 frag_color;

/* Bruit de valeur : haché sur les coins d'une maille, interpolé en douceur.
   Suffisant ici, on ne cherche qu'à casser la rondeur du disque. */
float hache(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float bruit(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f      = f * f * (3.0 - 2.0 * f);
    return mix(mix(hache(i), hache(i + vec2(1.0, 0.0)), f.x),
               mix(hache(i + vec2(0.0, 1.0)), hache(i + vec2(1.0, 1.0)), f.x),
               f.y);
}

void main() {
    vec2  p = v_uv - vec2(0.5);
    float d = length(p) * 2.0; /* 0 au centre, 1 au bord du quad */
    if (d > 1.0) {
        discard;
    }

    /* Bord fondu sur TOUT le rayon : une bouffée n'a pas de contour, et un
       plateau opaque au centre ferait voir les disques un par un au lieu d'un
       nuage. */
    float fondu = 1.0 - smoothstep(0.0, 0.95, d);

    /* Grain : deux octaves, décalées par la graine de l'instance. */
    vec2  q = v_uv * 4.0 + vec2(v_graine * 37.0, v_graine * 61.0);
    float n = 0.65 * bruit(q) + 0.35 * bruit(q * 2.7);

    float alpha = v_alpha * fondu * (0.55 + 0.75 * n);
    if (alpha < 0.004) {
        discard; /* rien de visible : inutile de mélanger ce pixel */
    }
    frag_color = vec4(v_couleur, alpha);
}
