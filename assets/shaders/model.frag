#version 410 core

// Échantillonne la texture diffuse et applique un éclairage de Lambert.

in vec3 v_normal;
in vec2 v_uv;

out vec4 frag_color;

uniform sampler2D u_texture;
uniform vec3      u_lightDir;  // direction VERS la lumière, normalisée
uniform float     u_opacity;   // 1 = opaque, < 1 = translucide (verrière)

void main() {
    vec4 albedo = texture(u_texture, v_uv);

    vec3  n       = normalize(v_normal);
    float diffuse = max(dot(n, normalize(u_lightDir)), 0.0);
    float ambient = 0.40;
    float light   = ambient + (1.0 - ambient) * diffuse;

    frag_color = vec4(albedo.rgb * light, albedo.a * u_opacity);
}
