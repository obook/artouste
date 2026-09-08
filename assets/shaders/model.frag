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
in vec3 v_pos;      /* repère modèle : les fêlures s'y accrochent, voir fissures() */

out vec4 frag_color;

uniform sampler2D u_texture;
uniform sampler2D u_relief;    /* carte de relief en espace tangent, unité 1 */
uniform int       u_hasRelief; /* 0 = la pièce n'en a pas, on garde sa normale */
uniform vec3      u_lightDir;  /* direction VERS la lumière, déjà normalisée */
uniform vec3      u_camPos;    /* position de la caméra (sert de lumière d'appoint) */
uniform float     u_opacity;   /* 1 = opaque, < 1 = translucide */
uniform int       u_glass;     /* 1 = vitrage : ajouter le reflet de Fresnel */
uniform float     u_damage;    /* 0 = verrière intacte, 1 = en miettes (mode zombie) */

/* Taille d'une écaille de verre. La verrière fait environ un mètre : à 22 cm,
   cinq ou six écailles en travers, ce qui se lit comme du verre feuilleté. */
const float ECAILLE_M = 0.22;

/* Germe d'une cellule, tiré au sort une fois pour toutes : la fêlure doit
   rester la même d'une image à l'autre. */
vec3 germe(vec3 cellule) {
    return fract(sin(vec3(dot(cellule, vec3(127.1, 311.7, 74.7)),
                          dot(cellule, vec3(269.5, 183.3, 246.1)),
                          dot(cellule, vec3(113.5, 271.9, 124.6)))) * 43758.5453);
}

/*
 * Réseau de fêlures : un Voronoï en repère modèle dont on ne garde que les
 * arêtes. Un vitrage casse en écailles jointives, pas en traits rayonnants.
 * Première boucle : le germe le plus proche. Seconde : la distance au plan
 * médiateur qui le sépare de ses voisins, nulle pile sur l'arête.
 *
 * Pas d'UV : les pièces vitrées n'ont pas de texture (voir Model::draw), donc
 * pas de dépliage fiable.
 *
 * 'degats' (0..1) ouvre la casse écaille par écaille : intacte à 0, tout est
 * fêlé à 1.
 */
float fissures(vec3 p, float degats) {
    vec3 pos = p / ECAILLE_M;
    vec3 base = floor(pos);
    vec3 f    = pos - base;

    vec3  gagnant     = vec3(0.0);  /* germe le plus proche, en coordonnées locales */
    vec3  celluleGagnante = base;
    float meilleure   = 8.0;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            for (int k = -1; k <= 1; ++k) {
                vec3  o = vec3(i, j, k);
                vec3  g = o + germe(base + o);
                float d = length(g - f);
                if (d < meilleure) {
                    meilleure       = d;
                    gagnant         = g;
                    celluleGagnante = base + o;
                }
            }
        }
    }

    /* Écaille encore intacte : rien à dessiner. */
    if (germe(celluleGagnante + 17.0).x > degats) {
        return 0.0;
    }

    float arete = 8.0;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            for (int k = -1; k <= 1; ++k) {
                vec3 o     = vec3(i, j, k);
                vec3 g     = o + germe(base + o);
                vec3 ecart = g - gagnant;
                if (dot(ecart, ecart) > 1e-5) {
                    arete = min(arete, dot(0.5 * (g + gagnant) - f, normalize(ecart)));
                }
            }
        }
    }

    /* Trait plus épais à mesure que ça casse, bords adoucis sur un pixel : la
       verrière est trop près de l'oeil pour être nette. */
    float demi = 0.02 + 0.05 * degats;
    float flou = max(fwidth(arete), 0.004);
    return 1.0 - smoothstep(demi - flou, demi + flou, arete);
}

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

        /* Verrière fêlée (mode zombie). Le trait opacifie le verre en plus de
           l'éclaircir, sinon il disparaît contre un ciel clair. Dessiné avec la
           pièce vitrée, il est masqué par le tableau de bord comme le reste. */
        if (u_damage > 0.0) {
            float trait = fissures(v_pos, u_damage);
            color = mix(color, vec3(0.92, 0.95, 1.00), trait);
            alpha = clamp(alpha + trait * 0.85, 0.0, 1.0);
        }
    }

    frag_color = vec4(color, alpha);
}
