#version 410 core

/*
 * building.frag
 * Éclairage de Lambert (lumière directionnelle + ambiant) sur la couleur du
 * sommet, puis brume vers l'horizon identique à celle du terrain, pour que les
 * bâtiments lointains se fondent dans le ciel au lieu de surgir nettement.
 *
 * Les murs (normale horizontale) sont en plus habillés d'une façade tuilée
 * échantillonnée en UV réels (voir BuildingsGeometrie.cpp) ; le toit (normale
 * vers le haut) module sa couleur de sommet (tirée de l'orthophoto) par le
 * relief de couverture. Le tri mur/toit se fait sur la normale, sans attribut
 * dédié : c'est la seule différence géométrique fiable entre les deux.
 *
 * u_facade est un atlas (tools/facade/generer_facade.py) : une colonne par
 * variante de mur, trois rangées (mur plein, rez-de-chaussée, étage, de bas en
 * haut une fois l'image retournée au chargement). La colonne vient de l'UV
 * vertical, par pas de FACADE_PAS_UV ; le mur plein (pignon) du SIGNE de l'UV
 * horizontal, négatif ; le rez-de-chaussée de l'étage, partie entière de l'UV
 * vertical. La structure Vertex est partagée avec le terrain, on n'y ajoute
 * pas d'attribut pour cela. La case est répétée par fract() et lue avec les
 * dérivées de l'UV continu (textureGrad), sinon le saut de fract casserait le
 * choix du mipmap sur la couture.
 * u_couverture a deux cases superposées : les rangs de tuiles (haut) pour les
 * pans, le gravier (bas) pour les toits plats.
 * La nuit, une part des carreaux s'allume (u_partAllumees, calculée d'après
 * l'heure par CycleJourNuit.cpp : la ville s'endort puis se rallume avant le
 * lever). Le tirage est fait ici et non dans la texture : celle-ci est tuilée en GL_REPEAT, une
 * fenêtre allumée dedans le serait sur tous les murs au même endroit. En
 * contrepartie, la géométrie des carreaux est redite ici -- si la tuile change
 * dans tools/facade/generer_facade.py, ces constantes suivent.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3 v_normal;
in vec3 v_color;
in vec2 v_uv;
in vec3 v_worldPos;
in vec3 v_pos;

out vec4 frag_color;

uniform sampler2D u_facade;     /* atlas des façades, murs seulement */
uniform sampler2D u_couverture; /* relief des toits, gris centré sur 0,5 */
uniform vec3  u_lightDir;   /* direction VERS la lumière, déjà normalisée */
uniform vec3  u_camPos;     /* position de la caméra (pour la distance de brume) */
uniform vec3  u_fogColor;   /* teinte de l'horizon vers laquelle on fond */
uniform float u_fogStart;   /* distance où la brume commence (m) */
uniform float u_fogEnd;     /* distance où tout est noyé dans la brume (m) */
uniform float u_isDay;      /* 1 en plein jour, 0 soleil couché (ApplicationRender.cpp) */
const float NUIT = 0.18;    /* part de l'éclairage qui reste la nuit (lune) */
uniform float u_partAllumees; /* part des fenêtres allumées à cette heure de la nuit */

/* Découpe de l'atlas et des carreaux, recopiée de generer_facade.py : BAYS
   cellules par case, un étage par case, la fenêtre occupant win_w/win_h au
   centre de chaque cellule (son centre à 0,55 de la hauteur). */
const vec2  ATLAS_CASES    = vec2(5.0, 3.0);  /* colonnes x rangées de l'atlas */
const float RANG_PLEIN     = 0.0;
const float RANG_RDC       = 1.0;
const float RANG_ETAGE     = 2.0;
const float FACADE_PAS_UV  = 100.0;           /* même valeur que BuildingsGeometrie.cpp */
const vec2  FACADE_GRILLE  = vec2(3.0, 1.0);  /* travées x étages par case */
/* Demi-carreau et centre par colonne de l'atlas : moitié de la largeur et de
   la hauteur de VARIANTES dans generer_facade.py. */
const vec2  FENETRE_DEMI[5]   = vec2[5](vec2(0.18, 0.30), vec2(0.22, 0.23), vec2(0.14, 0.32),
                                        vec2(0.28, 0.26), vec2(0.30, 0.24));
const vec2  FENETRE_CENTRE[5] = vec2[5](vec2(0.5, 0.55), vec2(0.5, 0.52), vec2(0.5, 0.56),
                                        vec2(0.5, 0.55), vec2(0.5, 0.55));
const float TRAVEE_M       = 4.0;             /* largeur d'une travée (12 m / 3 travées) */
const float ETAGE_M        = 3.0;             /* hauteur d'une case */

/* Ombre au pied des murs : un bâtiment posé au sol est sombre à sa base. */
const float OMBRE_PIED_M   = 2.0;
const float OMBRE_PIED     = 0.55;

/* Lit une case (col, rang) d'un atlas de nb cases, l'UV répété dedans. */
vec3 lireCase(sampler2D atlas, vec2 uv, vec2 nb, vec2 cas) {
    vec2 dans = (cas + fract(uv)) / nb;
    return textureGrad(atlas, dans, dFdx(uv) / nb, dFdy(uv) / nb).rgb;
}

/* Lueurs derrière un rideau : ampoule chaude, halogène, blanc neutre. */
const vec3  LUEURS[3] = vec3[3](vec3(1.00, 0.82, 0.48), vec3(1.00, 0.92, 0.70),
                                vec3(0.96, 0.94, 0.88));

/* Tirage au sort à partir d'une position entière : même entrée, même résultat,
   donc une fenêtre allumée le reste d'une image à l'autre. Hachage entier et
   non fract(sin(...)) : à dix kilomètres de l'origine, celui-ci perdait toute
   précision et allumait les mêmes carreaux sur toutes les maisons. */
float alea(ivec3 p, uint graine) {
    uint h = uint(p.x) * 374761393u + uint(p.y) * 668265263u + uint(p.z) * 2246822519u + graine;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return float(h ^ (h >> 16u)) / 4294967296.0;
}

void main() {
    vec3  n       = normalize(v_normal);

    /* Mur (normale horizontale, n.y ~ 0) : texture de façade, teintée par la
       couleur du sommet (légère variation par bâtiment). Toit (n.y ~ 1) :
       couleur du sommet seule (palette de toiture, pas de façade dessus). */
    float wallMask = 1.0 - step(0.5, abs(n.y));
    float plein    = step(v_uv.x, 0.0);
    float colonne  = floor(v_uv.y / FACADE_PAS_UV + 1e-3);
    vec2  uv       = vec2(v_uv.x, v_uv.y - colonne * FACADE_PAS_UV);
    float rang     = plein > 0.5 ? RANG_PLEIN : (uv.y < 1.0 ? RANG_RDC : RANG_ETAGE);
    vec3  tuile    = lireCase(u_facade, uv, ATLAS_CASES, vec2(colonne, rang));
    /* Pans en rangs de tuiles, toits plats en gravier : l'un ou l'autre, lu en
       gris et appliqué autour de 1 sur la couleur du toit. */
    float plat     = step(0.99, n.y);
    vec3  relief   = lireCase(u_couverture, v_uv, vec2(1.0, 2.0), vec2(0.0, plat > 0.5 ? 0.0 : 1.0));
    vec3  albedo   = mix(v_color * relief * 2.0, tuile * v_color, wallMask);

    float diffuse = max(dot(n, normalize(u_lightDir)), 0.0);
    float ambient = 0.4;
    float light   = (ambient + (1.0 - ambient) * diffuse) * mix(NUIT, 1.0, u_isDay);
    /* Ombre au pied du mur, sur la hauteur du rez-de-chaussée. */
    float pied    = mix(OMBRE_PIED, 1.0, smoothstep(0.0, OMBRE_PIED_M, uv.y * ETAGE_M));
    light        *= mix(1.0, pied, wallMask);
    vec3  color   = albedo * light;

    /* Fenêtres allumées, sur les seuls murs percés et seulement la nuit. La
       lueur remplace la couleur éclairée au lieu de s'y ajouter : un carreau
       allumé ne dépend plus de la lune qui tombe dessus, et ne peut pas non plus
       saturer en blanc. */
    float nuit = 1.0 - u_isDay;
    if (nuit > 0.0 && wallMask > 0.5 && plein < 0.5) {
        vec2 cellule = uv * FACADE_GRILLE;
        /* Bords adoucis sur la largeur d'un pixel : sans cela, les carreaux
           scintillent au loin, là où la texture de façade est déjà floutée par
           ses mipmaps alors que ce masque, lui, reste net. La dérivée est prise
           sur cellule et non sur sa partie fractionnaire, qui saute d'une
           cellule à l'autre et ferait une ligne blanche à chaque bord. */
        vec2 flou   = fwidth(cellule) * 0.5 + 1e-4;
        int  col    = int(colonne);
        vec2 d      = abs(fract(cellule) - FENETRE_CENTRE[col]);
        vec2 dedans = 1.0 - smoothstep(FENETRE_DEMI[col] - flou, FENETRE_DEMI[col] + flou, d);

        /* Identité de la fenêtre : le centre de sa cellule, en repère modèle.
           L'UV repart de zéro sur chaque mur (voir BuildingsGeometrie.cpp), donc
           le tirer sur la cellule seule allumerait les mêmes carreaux sur tous
           les bâtiments. Le mur étant vertical, sa tangente se déduit de sa
           normale. Le pas de 4 m entre deux centres laisse 2 m de marge à
           l'arrondi au mètre : tous les fragments d'un carreau tombent sur le
           même entier, donc sur le même tirage. */
        vec3  tangente = normalize(cross(vec3(0.0, 1.0, 0.0), n));
        vec3  centre   = v_pos - tangente * (fract(cellule.x) - 0.5) * TRAVEE_M;
        ivec3 cle      = ivec3(floor(centre.xz + 0.5), floor(cellule.y));
        float tirage   = alea(cle, 0u);
        /* Teinte et intensité propres à chaque fenêtre. */
        vec3  lueur    = LUEURS[int(alea(cle, 1u) * 3.0)] * (0.6 + 0.4 * alea(cle, 2u));

        float allumee = dedans.x * dedans.y * step(tirage, u_partAllumees);
        color = mix(color, lueur, allumee * nuit);
    }

    /* Brume : proportion croissante de couleur d'horizon avec la distance. */
    float dist = length(u_camPos - v_worldPos);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);

    frag_color = vec4(color, 1.0);
}
