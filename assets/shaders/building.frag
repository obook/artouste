#version 410 core

/*
 * building.frag
 * Éclairage de Lambert (lumière directionnelle + ambiant) sur la couleur du
 * sommet, puis brume vers l'horizon identique à celle du terrain, pour que les
 * bâtiments lointains se fondent dans le ciel au lieu de surgir nettement.
 *
 * Les murs (normale horizontale) sont en plus habillés d'une texture de façade
 * tuilée échantillonnée en UV réels (voir BuildingsMesh.cpp) ; le toit (normale
 * verticale) garde sa seule couleur de sommet (palette de tuiles/ardoise). Le
 * tri mur/toit se fait sur la normale, sans attribut dédié : c'est la seule
 * différence géométrique fiable entre les deux à ce stade.
 *
 * Deux tuiles de façade : la face ouverte (u_facade, fenêtres) et le pignon
 * aveugle (u_facadePleine). Le choix est fait par face au montage du maillage et
 * transporté par le SIGNE de l'UV horizontal, négatif pour un mur plein : la
 * structure Vertex est partagée avec le terrain, on n'y ajoute pas d'attribut
 * pour un seul bit. Les deux tuiles sont lues puis mélangées plutôt que lues
 * dans une branche, pour que les dérivées (donc le niveau de mipmap) restent
 * définies.
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

uniform sampler2D u_facade;       /* façade tuilée (fenêtres), murs seulement */
uniform sampler2D u_facadePleine; /* même tuile sans percement (pignon aveugle) */
uniform vec3  u_lightDir;   /* direction VERS la lumière, déjà normalisée */
uniform vec3  u_camPos;     /* position de la caméra (pour la distance de brume) */
uniform vec3  u_fogColor;   /* teinte de l'horizon vers laquelle on fond */
uniform float u_fogStart;   /* distance où la brume commence (m) */
uniform float u_fogEnd;     /* distance où tout est noyé dans la brume (m) */
uniform float u_isDay;      /* 1 en plein jour, 0 soleil couché (ApplicationRender.cpp) */
uniform float u_partAllumees; /* part des fenêtres allumées à cette heure de la nuit */

/* Découpe des carreaux dans la tuile de façade, recopiée de generer_facade.py :
   BAYS x FLOORS cellules, la fenêtre occupant win_w/win_h au centre de chacune. */
const vec2  FACADE_GRILLE = vec2(3.0, 2.0);   /* travées x étages par tuile */
const vec2  FENETRE_DEMI  = vec2(0.28, 0.26); /* demi-carreau, en fraction de cellule */
const float TRAVEE_M      = 4.0;              /* largeur d'une travée (12 m / 3 travées) */

/* Blanc chaud d'ampoule derrière un rideau, pas un néon. */
const vec3  LUEUR_FENETRE = vec3(1.00, 0.82, 0.48);

/* Tirage au sort à partir d'une position : même entrée, même résultat, donc une
   fenêtre allumée le reste d'une image à l'autre. */
float alea(vec3 p) {
    return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

void main() {
    vec3  n       = normalize(v_normal);

    /* Mur (normale horizontale, n.y ~ 0) : texture de façade, teintée par la
       couleur du sommet (légère variation par bâtiment). Toit (n.y ~ 1) :
       couleur du sommet seule (palette de toiture, pas de façade dessus). */
    float wallMask = 1.0 - step(0.5, abs(n.y));
    float plein    = step(v_uv.x, 0.0);
    vec3  tuile    = mix(texture(u_facade, v_uv).rgb,
                         texture(u_facadePleine, v_uv).rgb, plein);
    vec3  albedo   = mix(v_color, tuile * v_color, wallMask);

    float diffuse = max(dot(n, normalize(u_lightDir)), 0.0);
    float ambient = 0.4;
    float light   = ambient + (1.0 - ambient) * diffuse;
    vec3  color   = albedo * light;

    /* Fenêtres allumées, sur les seuls murs percés et seulement la nuit. La
       lueur remplace la couleur éclairée au lieu de s'y ajouter : un carreau
       allumé ne dépend plus de la lune qui tombe dessus, et ne peut pas non plus
       saturer en blanc. */
    float nuit = 1.0 - u_isDay;
    if (nuit > 0.0 && wallMask > 0.5 && plein < 0.5) {
        vec2 cellule = v_uv * FACADE_GRILLE;
        /* Bords adoucis sur la largeur d'un pixel : sans cela, les carreaux
           scintillent au loin, là où la texture de façade est déjà floutée par
           ses mipmaps alors que ce masque, lui, reste net. La dérivée est prise
           sur cellule et non sur sa partie fractionnaire, qui saute d'une
           cellule à l'autre et ferait une ligne blanche à chaque bord. */
        vec2 flou   = fwidth(cellule) * 0.5 + 1e-4;
        vec2 d      = abs(fract(cellule) - 0.5);
        vec2 dedans = 1.0 - smoothstep(FENETRE_DEMI - flou, FENETRE_DEMI + flou, d);

        /* Identité de la fenêtre : le centre de sa cellule, en repère modèle.
           L'UV repart de zéro sur chaque mur (voir BuildingsGeometrie.cpp), donc
           le tirer sur la cellule seule allumerait les mêmes carreaux sur tous
           les bâtiments. Le mur étant vertical, sa tangente se déduit de sa
           normale. Le pas de 4 m entre deux centres laisse 2 m de marge à
           l'arrondi au mètre : tous les fragments d'un carreau tombent sur le
           même entier, donc sur le même tirage. */
        vec3  tangente = normalize(cross(vec3(0.0, 1.0, 0.0), n));
        vec3  centre   = v_pos - tangente * (fract(cellule.x) - 0.5) * TRAVEE_M;
        float tirage   = alea(vec3(floor(centre.xz + 0.5), floor(cellule.y)));

        float allumee = dedans.x * dedans.y * step(tirage, u_partAllumees);
        color = mix(color, LUEUR_FENETRE, allumee * nuit);
    }

    /* Brume : proportion croissante de couleur d'horizon avec la distance. */
    float dist = length(u_camPos - v_worldPos);
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);

    frag_color = vec4(color, 1.0);
}
