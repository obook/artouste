#version 410 core

/*
 * zombie_eyes.frag
 * Lueur ronde procédurale (pas de texture) : un coeur presque blanc qui garde
 * la couleur de l'instance, entouré d'un halo qui s'éteint vers le bord du
 * quad. Dessinée en mélange additif (voir ZombieEyes::draw), donc l'alpha sert
 * de dosage : pas de découpe nette ici, contrairement aux pneus toxiques,
 * un bord franc trahirait le quad.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec2 v_uv;
in vec3 v_color;

out vec4 frag_color;

void main() {
    vec2  p = v_uv - vec2(0.5);
    float d = length(p) * 2.0;  /* 0 au centre, 1 au bord du quad */
    if (d > 1.0) {
        discard;
    }

    /* Halo large en 1/d^2 approché, plus un coeur net : deux termes valent mieux
       qu'un seul dégradé, qui donnerait soit un point dur, soit une brume. */
    float halo = pow(1.0 - d, 2.5);
    float core = smoothstep(0.45, 0.0, d);

    vec3 color = v_color * halo + vec3(1.0) * core * 0.6;
    frag_color = vec4(color, halo + core);
}
