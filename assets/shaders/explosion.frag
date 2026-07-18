#version 410 core

/*
 * explosion.frag
 * Feu émissif : on renvoie la couleur de la texture, destinée a etre AJOUTEE a
 * la scene (mélange additif réglé par ExplosionFx). Les zones sombres de la
 * texture n'ajoutent presque rien, les flammes vives éclaircissent. Un léger
 * gain rend la boule de feu plus punchy.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec2 v_uv;

out vec4 frag_color;

uniform sampler2D u_texture;
uniform float     u_fade;  /* enveloppe d'apparition/extinction (0..1) */

void main() {
    vec4 tex = texture(u_texture, v_uv);
    /* Gain émissif modéré ; l'alpha (module par le fondu) pilote la part ajoutée
       au mélange additif SRC_ALPHA, ONE. Gain réduit par rapport a avant pour
       éviter le blanc cramé. */
    frag_color = vec4(tex.rgb * 1.25, tex.a * u_fade);
}
