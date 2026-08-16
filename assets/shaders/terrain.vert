#version 410 core

/*
 * terrain.vert
 * Shader de sommets du terrain : transformation MVP, normale dans le repère
 * monde, position monde (pour la brume) et coordonnées de texture (UV) pour
 * draper l'orthophoto sur le relief.
 *
 * Il sert à deux dessins successifs du même terrain :
 *
 * - le maillage d'ensemble, dont les sommets arrivent en attributs ;
 * - la fenêtre de relief fin (u_reliefActif), qui n'a ni sommets ni normales en
 *   mémoire : ses points se déduisent de gl_VertexID et leur altitude est lue
 *   dans une texture torique (voir render/relief/FenetreRelief.hpp). Dessinée
 *   par-dessus, elle rejoint le relief d'ensemble sur les derniers mètres du
 *   bord plutôt que d'y découper un trou.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;
layout(location = 3) in vec2 a_uv;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

/* Fenêtre de relief fin. Éteinte pour le maillage d'ensemble, et sur toute carte
   sans tuiles de relief. */
uniform bool      u_reliefActif;
uniform sampler2D u_relief;        /* altitudes de la fenêtre, en mètres */
uniform vec2      u_reliefAncre;   /* coin nord-ouest de la tuile (0, 0), monde */
uniform float     u_reliefTailleM; /* période du tore au sol */
uniform float     u_reliefTexels;  /* côté de la texture, en texels */
uniform float     u_reliefPas;     /* maille de la fenêtre, en mètres */
uniform vec2      u_reliefCentre;  /* centre de la grille dessinée, monde */
uniform int       u_reliefCote;    /* côté de la grille dessinée, en points */
uniform vec2      u_reliefFondu;   /* début et fin du fondu vers la carte (m) */
uniform float     u_reliefLissage; /* niveau de réduction à la maille de la carte */
uniform float     u_reliefPasTexture;  /* maille des tuiles de relief (m) */
uniform float     u_reliefDetailM;     /* distance de pleine finesse (m) */

/* Relief d'ensemble en texture : la fenêtre s'y raccorde au bord. */
uniform sampler2D u_carteRelief;
uniform vec2      u_carteCoin;    /* coin nord-ouest de l'emprise, monde */
uniform vec2      u_carteTailleM; /* emprise au sol (largeur, hauteur) */
uniform vec2      u_carteTexels;  /* nombre de points de la heightmap */

out vec3 v_normal;
out vec2 v_uv;
out vec3 v_worldPos;

/* Détail apporté par la fenêtre : son altitude, moins cette même altitude lissée
   à la maille de la carte. Ce que la carte porte déjà est ainsi retranché, et
   seul le relief qu'elle ne peut pas tenir s'ajoute. Sans cela les deux relevés,
   qui ne s'accordent pas de plusieurs mètres, feraient pousser les sommets à
   l'entrée de la fenêtre. Le demi-texel tombe sur le point de grille lui-même. */
float detailFin(vec2 p, float finesse) {
    vec2 uv = (p - u_reliefAncre) / u_reliefTailleM + 0.5 / u_reliefTexels;
    return textureLod(u_relief, uv, finesse).r - textureLod(u_relief, uv, u_reliefLissage).r;
}

/* Finesse à laquelle lire le détail : pleine jusqu'à u_reliefDetailM, puis
   résolution divisée par deux à chaque doublement de distance, jusqu'à la maille
   de la carte, où le détail s'annule de lui-même.

   Elle ne dépend QUE de la distance, jamais de la grille : les deux grilles
   lisent donc la même surface là où elles se rejoignent. Et la loi est réglée
   (voir FenetreRelief::distanceDetailM) pour que la résolution lue tombe sur le
   pas de la grille qui la dessine : sans cela l'anneau échantillonne à 8 m une
   surface définie à 5,7 m, et il en naît des pics qui scintillent. */
float finesseDetail(float dist) {
    return clamp(log2(max(dist, 1.0) / u_reliefDetailM), 0.0, u_reliefLissage);
}

/* Altitude d'ensemble. Les points de la heightmap sont sur les BORDS de
   l'emprise, d'où le passage par (texels - 1). */
float hauteurCarte(vec2 p) {
    vec2 f  = (p - u_carteCoin) / u_carteTailleM;
    vec2 uv = (f * (u_carteTexels - 1.0) + 0.5) / u_carteTexels;
    return texture(u_carteRelief, uv).r;
}

/* Pas du gradient des normales du maillage d'ensemble, en mètres. Même valeur
   que Terrain.cpp, qui lisse l'ÉCLAIRAGE sans toucher au relief : la fenêtre
   doit retrouver exactement cette normale au bord, sinon le raccord fait une
   marche de lumière, un versant à l'ombre côtoyant le même versant au soleil. */
const float PAS_NORMALE_CARTE = 35.0;

/* Normale d'une surface échantillonnée à d mètres. X est, Z sud, Y en haut. */
vec3 normaleDe(float hl, float hr, float hn, float hs, float d) {
    return normalize(vec3(hl - hr, 2.0 * d, hn - hs));
}

/* Poids du fondu. MÊME formule que FenetreRelief::hauteurEn, qui porte le poser
   et la collision. */
float poidsFenetre(float dist) {
    return 1.0 - smoothstep(u_reliefFondu.x, u_reliefFondu.y, dist);
}

/* Pas du gradient de la normale : celui de la surface réellement lue tout près,
   celui de la carte au bord de la fenêtre.

   Il part de la finesse et NON du pas de la grille. Avec le pas de la grille,
   noyau et anneau éclairaient différemment le même point de leur frontière, et
   la marche de lumière suivait l'appareil. */
float pasNormale(float dist, float finesse) {
    return mix(u_reliefPasTexture * exp2(finesse), PAS_NORMALE_CARTE,
               smoothstep(0.0, u_reliefFondu.y, dist));
}

/* Altitude dessinée : celle de la carte, plus le détail de la fenêtre, qui
   s'efface au bord. Le poids est passé plutôt que recalculé : il ne varie que
   sur des centaines de mètres, et les cinq points que la normale demande sont
   voisins. MÊME formule que FenetreRelief::detailEn, qui porte le poser et la
   collision. */
float hauteurFenetre(vec2 p, float t, float finesse) {
    return hauteurCarte(p) + t * detailFin(p, finesse);
}

void main() {
    vec3 pos    = a_pos;
    vec3 normal = a_normal;
    vec2 uv     = a_uv;

    if (u_reliefActif) {
        /* Point de la grille, en monde : rien n'est lu, la grille n'a que des
           indices. */
        int   i = gl_VertexID % u_reliefCote;
        int   j = gl_VertexID / u_reliefCote;
        float demi = 0.5 * float(u_reliefCote - 1) * u_reliefPas;
        vec2  p    = u_reliefCentre + vec2(float(i), float(j)) * u_reliefPas - demi;

        float dist    = distance(p, u_reliefCentre);
        float t       = poidsFenetre(dist);
        float finesse = finesseDetail(dist);
        pos           = vec3(p.x, hauteurFenetre(p, t, finesse), p.y);

        /* Normale par différences finies sur la surface dessinée. Au bord, le pas
           et la hauteur sont ceux de la carte : les deux se raccordent en lumière
           comme en altitude. */
        float d = pasNormale(dist, finesse);
        normal  = normaleDe(hauteurFenetre(p + vec2(-d, 0.0), t, finesse),
                            hauteurFenetre(p + vec2(d, 0.0), t, finesse),
                            hauteurFenetre(p + vec2(0.0, -d), t, finesse),
                            hauteurFenetre(p + vec2(0.0, d), t, finesse), d);

        /* Même drapage que le maillage d'ensemble, V comprise : il écrit
           1 - j / (rows - 1), la rangée 0 étant au nord. Sans cette inversion,
           la fenêtre drapait l'orthophoto retournée nord-sud. */
        vec2 f = (p - u_carteCoin) / u_carteTailleM;
        uv     = vec2(f.x, 1.0 - f.y);
    }

    vec4 world  = u_model * vec4(pos, 1.0);
    v_worldPos  = world.xyz;
    v_normal    = mat3(u_model) * normal;
    v_uv        = uv;
    gl_Position = u_proj * u_view * world;
}
