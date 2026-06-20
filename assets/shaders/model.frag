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

    vec3  color = albedo.rgb * min(light, 1.2);
    float alpha = albedo.a * u_opacity;

    /*
     * Verrière (u_opacity < 1) : une vitre vue de face est presque invisible, au
     * point de sembler absente. On ajoute un reflet de Fresnel (plus fort aux
     * angles rasants) qui éclaircit et opacifie les bords : la cabine se lit
     * alors comme du verre depuis l'intérieur, sans assombrir la vue de face.
     * abs() rend l'effet identique des deux côtés de la vitre.
     */
    if (u_opacity < 0.999) {
        float fresnel = pow(1.0 - abs(dot(n, viewDir)), 3.0);
        color = mix(color, vec3(0.85, 0.90, 0.95), fresnel * 0.6);
        alpha = clamp(alpha + fresnel * 0.5, 0.0, 1.0);
    }

    frag_color = vec4(color, alpha);
}
