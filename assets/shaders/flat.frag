#version 410 core

/*
 * flat.frag
 * Remplit chaque pixel avec une couleur uniforme, alpha compris, ce qui permet
 * par exemple de dessiner une ombre portée translucide.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

out vec4 frag_color;

uniform vec4 u_color;

void main() {
    frag_color = u_color;
}
