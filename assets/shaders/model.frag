#version 410 core

/*
 * model.frag
 * Lit la couleur dans la texture du modèle, puis applique un éclairage de Lambert
 * adouci ainsi qu'une lumière d'appoint venant de la caméra.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_worldPos;

out vec4 frag_color;

uniform sampler2D u_texture;
uniform vec3      u_lightDir;  /* direction VERS la lumière, déjà normalisée */
uniform vec3      u_camPos;    /* position de la caméra (sert de lumière d'appoint) */
uniform float     u_opacity;   /* 1 = opaque, < 1 = translucide (verrière) */

void main() {
    vec4 albedo = texture(u_texture, v_uv);

    /* Demi-Lambert : adoucit l'ombre sans aplatir le relief. */
    vec3  n       = normalize(v_normal);
    float diffuse = dot(n, normalize(u_lightDir)) * 0.5 + 0.5;

    /* Lumière d'appoint depuis la caméra : éclaire ce que l'on regarde (par
       exemple la planche de bord, à l'intérieur, dans l'ombre de la cellule). */
    vec3  viewDir   = normalize(u_camPos - v_worldPos);
    float headlight = max(dot(n, viewDir), 0.0);

    float light = 0.40 + 0.45 * diffuse + 0.45 * headlight;

    frag_color = vec4(albedo.rgb * min(light, 1.2), albedo.a * u_opacity);
}
