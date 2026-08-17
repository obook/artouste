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
uniform vec2      u_reliefTailleM; /* période du tore au sol, par axe */
uniform float     u_reliefTexels;  /* côté de la texture, en texels */
uniform vec2      u_reliefPas;     /* maille de la fenêtre, par axe */
uniform vec2      u_reliefCentre;  /* centre de la grille dessinée, monde */
uniform vec2      u_reliefOeil;    /* position continue de l'appareil, monde */
uniform int       u_reliefCote;    /* côté de la grille dessinée, en points */
uniform vec2      u_reliefFondu;   /* début et fin du fondu vers la carte (m) */
uniform float     u_reliefLissage; /* niveau de réduction à la maille de la carte */
uniform vec2      u_reliefPasTexture;  /* maille des tuiles de relief, par axe */
uniform float     u_reliefDetailM;     /* distance de pleine finesse (m) */

/* Relief d'ensemble en texture : la fenêtre s'y raccorde au bord. */
uniform sampler2D u_carteRelief;
uniform vec2      u_carteCoin;    /* coin nord-ouest de l'emprise, monde */
uniform vec2      u_carteTailleM; /* emprise au sol (largeur, hauteur) */
uniform vec2      u_carteTexels;  /* nombre de points de la heightmap */
uniform int       u_cartePasMaillage; /* décimation du maillage d'ensemble */
uniform float     u_cartePasNormale;  /* pas réel de ses normales (m) */
uniform bool      u_reliefRaccord;    /* mise au point : débraye le raccord */

out vec3 v_normal;
out vec2 v_uv;
out vec3 v_worldPos;

/* Altitude d'ensemble. Les points de la heightmap sont sur les BORDS de
   l'emprise, d'où le passage par (texels - 1). */
float hauteurCarte(vec2 p) {
    vec2 f  = (p - u_carteCoin) / u_carteTailleM;
    vec2 uv = (f * (u_carteTexels - 1.0) + 0.5) / u_carteTexels;
    return texture(u_carteRelief, uv).r;
}

/* Détail apporté par la fenêtre : l'écart entre le laser et LA CARTE, pas entre
   le laser et son propre lissé. Ainsi carte + détail vaut exactement le laser,
   par construction.

   L'ancienne forme retranchait le laser lissé : la carte portant déjà la crête,
   on lui ajoutait la crête du laser au-dessus de son propre lissé, donc on la
   comptait DEUX FOIS. Les sommets en sortaient surdessinés, jusqu'à 15 m au 99e
   centile sur les crêtes de bigorre, contre 5 m d'écart réel entre laser et
   carte. Le demi-texel tombe sur le point de grille lui-même. */
float detailFin(vec2 p, float finesse) {
    vec2 uv = (p - u_reliefAncre) / u_reliefTailleM + 0.5 / u_reliefTexels;
    return textureLod(u_relief, uv, finesse).r - hauteurCarte(p);
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

/* Surface RÉELLE du maillage d'ensemble : ses points RETENUS et sa découpe en
   triangles, pas la carte lue en bilinéaire. Il ne garde qu'un point sur
   u_cartePasMaillage, plus le dernier de chaque axe, et coupe chaque maille
   du coin nord-est au coin sud-ouest (Terrain.cpp : a, c, b puis b, c, d).

   C'est vers CETTE surface que le bord de la fenêtre doit converger. Vers la
   carte bilinéaire, il resterait un écart sous le mètre, assez pour décaler les
   silhouettes et faire "se dessiner" les crêtes au passage de la frontière. */
float hauteurMaillage(vec2 p) {
    vec2  f   = (p - u_carteCoin) / u_carteTailleM;
    vec2  g   = f * (u_carteTexels - 1.0);
    float pas = float(max(u_cartePasMaillage, 1));
    vec2  i0  = floor(g / pas) * pas;
    vec2  i1  = min(i0 + pas, u_carteTexels - 1.0);
    vec2  t   = clamp((g - i0) / max(i1 - i0, vec2(1.0)), 0.0, 1.0);
    float h00 = texture(u_carteRelief, (i0 + 0.5) / u_carteTexels).r;
    float h10 = texture(u_carteRelief, (vec2(i1.x, i0.y) + 0.5) / u_carteTexels).r;
    float h01 = texture(u_carteRelief, (vec2(i0.x, i1.y) + 0.5) / u_carteTexels).r;
    float h11 = texture(u_carteRelief, (i1 + 0.5) / u_carteTexels).r;
    return (t.x + t.y <= 1.0)
               ? (1.0 - t.x - t.y) * h00 + t.x * h10 + t.y * h01
               : (1.0 - t.y) * h10 + (1.0 - t.x) * h01 + (t.x + t.y - 1.0) * h11;
}

/* Normale d'une surface échantillonnée à d mètres. X est, Z sud, Y en haut. */
vec3 normaleDe(float hl, float hr, float hn, float hs, float d) {
    return normalize(vec3(hl - hr, 2.0 * d, hn - hs));
}

/* Poids du fondu. MÊME formule que FenetreRelief::hauteurEn, qui porte le poser
   et la collision. */
float poidsFenetre(float dist) {
    return 1.0 - smoothstep(u_reliefFondu.x, u_reliefFondu.y, dist);
}

/* Pas du gradient de la normale : celui du CHAMP RÉELLEMENT LU, qui montre tout
   ce que le champ contient et rien de plus. Il ne rejoint le pas de la carte que
   dans la bande de fondu, où la lumière doit se raccorder au maillage
   d'ensemble.

   Il part de la finesse et NON du pas de la grille. Avec le pas de la grille,
   noyau et anneau éclairaient différemment le même point de leur frontière.

   Le mélange part de fonduDebut et non de zéro : en partant de zéro, l'éclairage
   d'un versant changeait tout au long de l'approche, et la carte de lumière
   glissait sur le terrain en suivant l'appareil. */
float pasNormale(float dist, float finesse) {
    return mix(0.5 * (u_reliefPasTexture.x + u_reliefPasTexture.y) * exp2(finesse),
               u_cartePasNormale,
               smoothstep(u_reliefFondu.x, u_reliefFondu.y, dist));
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
        vec2 demi = 0.5 * float(u_reliefCote - 1) * u_reliefPas;
        vec2 p     = u_reliefCentre + vec2(float(i), float(j)) * u_reliefPas - demi;

        /* Le centre calé pose les sommets, l'oeil mesure les champs. */
        float dist    = distance(p, u_reliefOeil);
        float t       = poidsFenetre(dist);
        float finesse = finesseDetail(dist);
        pos           = vec3(p.x, hauteurFenetre(p, t, finesse), p.y);

        /* Le bord rejoint la surface du maillage, pas la carte bilinéaire : à la
           fin du fondu les deux sont ÉGALES. Sans cela il reste un écart sous le
           mètre, assez pour décaler les silhouettes au passage de la frontière. */
        if (t < 1.0 && u_reliefRaccord) {
            pos.y += (1.0 - t) * (hauteurMaillage(p) - hauteurCarte(p));
        }

        /* Normale par différences finies sur la carte PLEINE, jamais sur la
           surface décimée : c'est ainsi que le maillage calcule les siennes
           (Terrain.cpp, voisins à u_cartePasNormale mètres). */
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
