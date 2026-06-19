#version 410 core

// Éclairage diffus (Lambert) d'une seule source directionnelle + terme ambiant.

in vec3 v_normal;
in vec3 v_color;

out vec4 frag_color;

uniform vec3 u_lightDir;  // direction VERS la lumière, normalisée

void main() {
    vec3  n       = normalize(v_normal);
    float diffuse = max(dot(n, normalize(u_lightDir)), 0.0);
    float ambient = 0.35;
    float light   = ambient + (1.0 - ambient) * diffuse;
    frag_color    = vec4(v_color * light, 1.0);
}
