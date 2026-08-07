#version 140
#extension GL_ARB_explicit_attrib_location : require

/*
 * souffle.vert
 * Bouffées de poussière du souffle rotor, en billboards face caméra instanciés
 * (même principe que projectile.vert), avec une rotation propre à chaque
 * bouffée pour casser la répétition.
 *
 * La COULEUR est prise sur l'orthophoto du terrain, au point même où la bouffée
 * se trouve : la poussière est ocre sur une piste, grise sur un éboulis et
 * blanche sur la neige, sans rien avoir à régler carte par carte. Le
 * prélèvement se fait dans un niveau de réduction (textureLod) pour obtenir la
 * couleur MOYENNE du sol alentour plutôt que le détail d'un pixel.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec2 a_corner; /* coin du quad, x/y dans [-0.5, 0.5] */
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_centreDiametre; /* xyz centre monde, w diamètre (m) */
layout(location = 3) in vec4 a_grain; /* x opacité, y graine, z rotation (rad), w hauteur sol (m) */

uniform mat4 u_model; /* recalage repère-caméra (translation -origine de rendu) */
uniform mat4 u_view;
uniform mat4 u_proj;

uniform sampler2D u_ortho;      /* orthophoto du terrain (couleur du sol) */
uniform vec2      u_orthoMin;   /* coin nord-ouest de l'emprise, en coordonnées monde */
uniform vec2      u_orthoTaille; /* emprise (largeur, hauteur) en m ; 0 = pas d'orthophoto */
uniform vec3      u_lightDir;   /* direction VERS le soleil, normalisée */
uniform vec3      u_camPos;     /* position caméra (repère recalé), pour la brume */
uniform vec3      u_fogColor;
uniform float     u_fogStart;
uniform float     u_fogEnd;

out vec2  v_uv;
out float v_alpha;
out float v_graine;
out vec3  v_couleur;

/* Poussière par défaut, quand la carte n'a pas d'orthophoto (terrain de repli). */
const vec3 POUSSIERE_DEFAUT = vec3(0.62, 0.55, 0.44);
/* Embruns : au-dessus de l'eau, ce n'est plus de la terre qui monte. Gris très
   clair plutôt que blanc pur : sur un lac sombre, le contraste est déjà fort. */
const vec3 EMBRUNS = vec3(0.78, 0.82, 0.85);

void main() {
    vec3 centre = (u_model * vec4(a_centreDiametre.xyz, 1.0)).xyz;

    /* Coin du quad, tourné de l'angle propre à la bouffée. */
    float c    = cos(a_grain.z);
    float s    = sin(a_grain.z);
    vec2  coin = vec2(a_corner.x * c - a_corner.y * s, a_corner.x * s + a_corner.y * c);

    /* Billboard face caméra, comme les autres effets du jeu (une bouffée couchée
       à plat disparaîtrait vue de la caméra de poursuite, qui rase le sol). */
    vec3 rightCam = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 upCam    = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
    vec3 pos      = centre + (rightCam * coin.x + upCam * coin.y) * a_centreDiametre.w;

    /* Un quad vertical plus grand que sa garde au sol traverse le terrain, et le
       test de profondeur le coupe net : toutes les bouffées de la même hauteur
       étant coupées sur la même ligne, un trait barre le nuage en travers. On
       efface donc progressivement celles qui sont trop grosses pour la place
       qu'elles ont sous elles, ce qui revient aussi à ce qu'on voit en vrai :
       la poussière s'affirme en montant, pas au ras du sol. */
    float degagement = a_grain.w / max(0.5 * a_centreDiametre.w, 0.01);
    float visible    = smoothstep(0.10, 0.70, degagement);

    /* Et symétriquement, une bouffée qui se retrouve LOIN au-dessus du terrain
       n'est plus de la poussière soulevée : c'est de la fumée en suspension.
       Le cas se produit sur une hélisurface perchée (Ossau), où la nappe qui
       s'écarte quitte le plateau et surplombe soudain le fond de vallée : sans
       cette extinction, le pad se retrouvait entouré de colonnes qui montaient
       bien plus haut que l'appareil. Sur terrain plat, elle ne fait qu'écrêter
       le sommet du panache, qui dépasse rarement trois mètres. */
    visible *= 1.0 - smoothstep(3.0, 7.0, a_grain.w);

    /* Couleur du sol sous la bouffée. Le calage reprend celui du maillage du
       terrain (voir Terrain::build) : rangée 0 de l'orthophoto = nord, donc V
       est inversé. */
    vec3 couleur = POUSSIERE_DEFAUT;
    if (u_orthoTaille.x > 0.0) {
        vec2 uv = (a_centreDiametre.xz - u_orthoMin) / u_orthoTaille;
        uv      = clamp(vec2(uv.x, 1.0 - uv.y), vec2(0.0), vec2(1.0));
        vec3 sol = textureLod(u_ortho, uv, 3.0).rgb;
        /* La poussière en suspension est plus claire que le sol vu du dessus :
           elle diffuse la lumière au lieu de l'absorber. C'est cet éclaircissement
           qui la fait exister, sans quoi, sur un sol clair (piste, sable), elle
           aurait exactement la teinte du fond et resterait invisible quelle que
           soit son opacité.
           On ne la désature qu'à peine (le mélange de particules atténue un peu
           la couleur locale, il ne l'efface pas) : trop tirée vers le gris, elle
           tournait à la fumée et perdait l'ocre qu'on est justement allé chercher
           dans l'orthophoto. Le léger biais chaud va dans le même sens, la terre
           soulevée est plus jaune que le sol qu'on regarde d'en haut. */
        float luminance = dot(sol, vec3(0.299, 0.587, 0.114));
        couleur         = mix(sol, vec3(luminance), 0.15) * vec3(1.46, 1.40, 1.28) + 0.09;

        /* Eau : même signature de couleur que le masque de la végétation (sombre
           et peu saturée, voir VegetationWaterMask.cpp). On ne soulève pas de
           terre sur un lac, on lève des embruns. Le critère se trompe sur un
           versant à l'ombre profonde, ce qui blanchit un peu la poussière : sans
           conséquence, la teinte reste proche du sol. */
        float hi  = max(sol.r, max(sol.g, sol.b));
        float lo  = min(sol.r, min(sol.g, sol.b));
        float eau = (1.0 - smoothstep(0.30, 0.38, hi)) * (1.0 - smoothstep(0.08, 0.12, hi - lo));
        couleur   = mix(couleur, EMBRUNS, eau);
        /* Les embruns tranchent bien plus sur l'eau sombre que la poussière sur
           un sol clair : à opacité égale, le nuage passerait de discret à énorme
           tache blanche d'une carte à l'autre. On l'allège donc d'autant. */
        visible *= mix(1.0, 0.35, eau);
    }

    /* Luminosité selon la hauteur du soleil, comme la végétation. */
    float jour = clamp(u_lightDir.y * 0.5 + 0.5, 0.25, 1.0);

    /* Brume : calculée par bouffée (elles sont petites, le dégradé dans le quad
       ne se verrait pas). */
    float dist = length(u_camPos - centre);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);

    /* Bouffée collée à l'objectif : elle couvrirait l'écran entier à elle seule,
       en vue cockpit surtout, et quelques centaines de quads plein écran coûtent
       cher en remplissage sur une machine modeste. On les efface de près, ce qui
       est aussi ce qu'on voit : on ne distingue pas un nuage le nez dedans. */
    visible *= smoothstep(0.8, 2.5, dist);

    v_uv        = a_uv;
    v_alpha     = a_grain.x * visible;
    v_graine    = a_grain.y;
    v_couleur   = mix(couleur * jour, u_fogColor, fog);
    gl_Position = u_proj * u_view * vec4(pos, 1.0);
}
