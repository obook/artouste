#version 410 core

// Couleur uniforme avec alpha (ombre portée translucide).

out vec4 frag_color;

uniform vec4 u_color;

void main() {
    frag_color = u_color;
}
