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

    // Demi-Lambert : adoucit l'ombre (les faces opposées à la lumière, comme la
    // planche de bord à l'intérieur, restent éclairées) sans aplatir le relief.
    vec3  n       = normalize(v_normal);
    float ndl     = dot(n, normalize(u_lightDir));
    float diffuse = ndl * 0.5 + 0.5;
    float light   = 0.32 + 0.68 * diffuse;

    frag_color = vec4(albedo.rgb * light, albedo.a * u_opacity);
}
