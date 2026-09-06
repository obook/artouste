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
uniform sampler2D u_relief;    /* carte de relief en espace tangent, unité 1 */
uniform int       u_hasRelief; /* 0 = la pièce n'en a pas, on garde sa normale */
uniform vec3      u_lightDir;  /* direction VERS la lumière, déjà normalisée */
uniform vec3      u_camPos;    /* position de la caméra (sert de lumière d'appoint) */
uniform float     u_opacity;   /* 1 = opaque, < 1 = translucide */
uniform int       u_glass;     /* 1 = vitrage : ajouter le reflet de Fresnel */

/*
 * Repère tangent reconstruit dans le fragment, à partir des dérivées d'écran de
 * la position et des coordonnées de texture. On évite ainsi de stocker une
 * tangente par sommet : la structure Vertex est partagée avec le terrain, dont
 * la grille fait un million de sommets qui n'auront jamais de carte de relief.
 * Ce repère suit le dépliage réel au pixel près, donc les îlots en miroir et la
 * convention de l'outil de cuisson ne posent pas de question de signe.
 * Carte attendue en convention OpenGL (canal vert vers le haut), celle que
 * Blender produit.
 */
vec3 relief(vec3 n, vec3 worldPos, vec2 uv) {
    vec3 dpx = dFdx(worldPos);
    vec3 dpy = dFdy(worldPos);
    vec2 dux = dFdx(uv);
    vec2 duy = dFdy(uv);

    vec3  perpY  = cross(dpy, n);
    vec3  perpX  = cross(n, dpx);
    vec3  t      = perpY * dux.x + perpX * duy.x;
    vec3  b      = perpY * dux.y + perpX * duy.y;
    float invMax = inversesqrt(max(dot(t, t), dot(b, b)));

    vec3 m = texture(u_relief, uv).xyz * 2.0 - 1.0;
    return normalize(mat3(t * invMax, b * invMax, n) * m);
}

void main() {
    vec4 albedo = texture(u_texture, v_uv);

    /* Demi-Lambert : adoucit l'ombre sans aplatir le relief. */
    vec3  n       = normalize(v_normal);
    if (u_hasRelief != 0) {
        n = relief(n, v_worldPos, v_uv);
    }
    float diffuse = dot(n, normalize(u_lightDir)) * 0.5 + 0.5;

    /* Lumière d'appoint depuis la caméra : éclaire ce que l'on regarde (par
       exemple la planche de bord, à l'intérieur, dans l'ombre de la cellule). */
    vec3  viewDir   = normalize(u_camPos - v_worldPos);
    float headlight = max(dot(n, viewDir), 0.0);

    float light = 0.40 + 0.45 * diffuse + 0.45 * headlight;

    /* Reflets : la tôle peinte renvoie le soleil en un éclat serré (Blinn-Phong) et
       le ciel sur ses bords, là où le regard rase la surface. Sans ces deux termes,
       la cellule rend comme du plastique mat quelle que soit la finesse du maillage.
       L'éclat du soleil est coupé du côté à l'ombre, que le demi-Lambert éclaire
       encore ; le reflet du ciel, lui, ne dépend pas de l'heure. */
    vec3  lightDir = normalize(u_lightDir);
    vec3  halfDir  = normalize(lightDir + viewDir);
    float lit      = clamp(dot(n, lightDir) * 4.0, 0.0, 1.0);
    float soleil   = pow(max(dot(n, halfDir), 0.0), 32.0) * 0.60 * lit;
    float ciel     = pow(1.0 - max(dot(n, viewDir), 0.0), 4.0) * 0.18;

    vec3  color = albedo.rgb * min(light, 1.2) + vec3(soleil) +
                  vec3(0.55, 0.68, 0.85) * ciel;
    float alpha = albedo.a * u_opacity;

    /*
     * Verrière : une vitre vue de face est presque invisible, au point de sembler
     * absente. On ajoute un reflet de Fresnel (plus fort aux angles rasants) qui
     * éclaircit et opacifie les bords : la cabine se lit alors comme du verre
     * depuis l'intérieur, sans assombrir la vue de face. abs() rend l'effet
     * identique des deux côtés de la vitre.
     */
    if (u_glass == 1) {
        float fresnel = pow(1.0 - abs(dot(n, viewDir)), 3.0);
        color = mix(color, vec3(0.85, 0.90, 0.95), fresnel * 0.6);
        alpha = clamp(alpha + fresnel * 0.5, 0.0, 1.0);
    }

    frag_color = vec4(color, alpha);
}
