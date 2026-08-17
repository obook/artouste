#version 410 core

/*
 * terrain.frag
 * Drape l'orthophoto réelle sur le relief : couleur lue dans la texture,
 * éclairage de Lambert adouci (pour faire ressortir les pentes), puis brume
 * vers l'horizon qui fait fondre le lointain dans la couleur du ciel. La brume
 * masque aussi le bord du terrain et le plan de coupe lointain de la caméra.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_worldPos;

out vec4 frag_color;

uniform sampler2D u_texture;
uniform sampler2D u_detail;     /* grain rocheux tuilable, unité 1 */
/* Tuiles d'orthophoto fine : deux niveaux au plus, du plus large au plus serré
   (voir render/tuiles/Fenetre.hpp). Le niveau large couvre l'emprise de la
   carte, le niveau serré n'existe qu'aux abords des aires de poser, là où l'on
   descend assez bas pour que la finesse du large ne suffise plus. */
uniform sampler2D u_fine;        /* fenêtre du niveau large, unité 2 */
uniform sampler2D u_fineMasque;  /* résidence par tuile, unité 3 */
uniform vec2      u_fineAncre;   /* coin nord-ouest de la grille (monde) */
uniform float     u_fineTailleM; /* côté de la fenêtre au sol : période du tore */
uniform float     u_fineMPP;     /* finesse des tuiles (m au sol par pixel) */
uniform float     u_finePlein;   /* distance jusqu'où le détail est plein (m) */
uniform float     u_fineFondu;   /* distance où le détail a disparu (m) */
uniform sampler2D u_serre;        /* idem pour le niveau serré, unités 4 et 5 */
uniform sampler2D u_serreMasque;
uniform vec2      u_serreAncre;
uniform float     u_serreTailleM;
uniform float     u_serreMPP;
uniform float     u_serrePlein;
uniform float     u_serreFondu;
uniform vec3      u_lightDir;   /* direction VERS la lumière, déjà normalisée */
uniform vec3      u_seaColor;   /* couleur du plan de mer, pour fondre le bord */
uniform vec3      u_camPos;     /* position de la caméra (pour la distance de brume) */
uniform vec3      u_fogColor;   /* teinte de l'horizon vers laquelle on fond */
uniform float     u_fogStart;   /* distance où la brume commence (m) */
uniform float     u_fogEnd;     /* distance où tout est noyé dans la brume (m) */
uniform vec2      u_originXZ;   /* origine de rendu : reconstitue le monde absolu */
uniform float     u_orthoMPP;   /* finesse de l'orthophoto (m au sol par pixel) */

/* Sonde de mise au point (ARTOUSTE_DEBUG_SONDE) : 3 écrit l'altitude du sol au
   lieu de sa couleur. À retirer. */
uniform int       u_sonde;

/* Pose un niveau de tuiles sur la couleur du sol et renvoie la part réellement
   appliquée (0 = couleur inchangée).

   La fenêtre est un tore : les coordonnées monde s'y ramènent par une simple
   division par sa période, la répétition de la texture faisant le reste (voir
   render/tuiles/Fenetre.hpp). On ne prend surtout pas le reste fractionnaire
   soi-même : la discontinuité qu'il introduirait ferait exploser les dérivées
   des coordonnées, et le GPU choisirait le niveau de réduction le plus
   grossier, traçant une couture floue le long de la couture du tore.

   Le masque de résidence dit, emplacement par emplacement, si la tuile attendue
   est bien arrivée : sans lui on afficherait des blocs vides, ou la tuile d'un
   autre bout de la carte. Une période nulle éteint le niveau, ce qui est le cas
   normal d'une carte sans tuiles : repli sans danger. */
float poserTuiles(sampler2D tuiles, sampler2D masque, vec2 monde, vec2 ancre,
                  float tailleM, float plein, float fondu, float dist,
                  inout vec3 couleur) {
    if (tailleM <= 0.0) {
        return 0.0;
    }
    vec2  uv   = (monde - ancre) / tailleM;
    float part = texture(masque, uv).r * (1.0 - smoothstep(plein, fondu, dist));
    couleur    = mix(couleur, texture(tuiles, uv).rgb, part);
    return part;
}

void main() {
    if (u_sonde == 3 || u_sonde == 4) {
        /* 3 : altitude du sol. 4 : distance à la caméra. En centimètres sur
           24 bits. La 4 dit si deux rendus qui divergent montrent le même
           morceau de terrain ou deux morceaux différents. */
        float v = (u_sonde == 3) ? clamp((v_worldPos.y + 1000.0) * 100.0, 0.0, 16777215.0)
                                 : clamp(length(u_camPos - v_worldPos) * 100.0, 0.0, 16777215.0);
        frag_color = vec4(floor(v / 65536.0) / 255.0,
                          floor(mod(v, 65536.0) / 256.0) / 255.0,
                          floor(mod(v, 256.0)) / 255.0, 1.0);
        return;
    }

    vec3 ortho = texture(u_texture, v_uv).rgb;

    float dist  = length(u_camPos - v_worldPos);
    vec2  monde = v_worldPos.xz + u_originXZ;

    /* Du plus large au plus serré : chaque niveau recouvre le précédent là où
       ses tuiles sont présentes et où l'on est assez près pour qu'il apporte
       quelque chose. */
    float fine  = poserTuiles(u_fine, u_fineMasque, monde, u_fineAncre,
                              u_fineTailleM, u_finePlein, u_fineFondu, dist, ortho);
    float serre = poserTuiles(u_serre, u_serreMasque, monde, u_serreAncre,
                              u_serreTailleM, u_serrePlein, u_serreFondu, dist, ortho);

    /* Finesse effective sous ce fragment : celle du niveau qui s'y applique
       réellement, l'orthophoto d'ensemble à défaut. C'est elle, et non la seule
       orthophoto, qui doit doser le grain de synthèse plus bas. */
    float finesse = mix(mix(u_orthoMPP, u_fineMPP, fine), u_serreMPP, serre);

    /* Grain rocheux de près : une texture de détail tuilée, échantillonnée en
       coordonnées MONDE ABSOLUES (v_worldPos est relatif à l'origine de rendu
       flottante : sans u_originXZ le motif glisserait à chaque rebasage), à
       DEUX échelles (tuiles de 1,5 m et 11 m) : la grande module la petite et
       casse la répétition, qui rayait les pentes en velours côtelé avec une
       seule tuile. Force = pente x proximité : plein sur les faces raides
       (> ~30 degrés), plancher discret partout pour casser l'aplat de
       l'ortho ; fondu éteint dès 1200 m, car au-delà la tuile de 11 m ne
       couvre plus que quelques pixels et sa répétition quadrillait les
       versants d'en face, alors que l'ortho seule y suffit largement.
       La modulation se fait autour de 1 : les couleurs IGN restent. */
    vec3  n      = normalize(v_normal);
    float detail = 0.6 * texture(u_detail, monde / 1.5).r
                 + 0.4 * texture(u_detail, monde / 11.0).r;
    float pente  = smoothstep(0.08, 0.30, 1.0 - n.y);
    /* Plancher du grain sur le PLAT : il n'existe que pour casser l'aplat d'une
       orthophoto qui n'a pas de détail propre à montrer de près. Sous ~0,3 m/px
       (dax, dax-arene) la photo a son vrai grain et le motif de synthèse ne
       compense plus rien : il pose au contraire un tissage régulier de 1,5 m,
       très visible sur une surface uniforme comme une plage vue de 30 m. On
       l'efface donc là, et seulement là.

       Le seuil a d'abord été posé à 0,8 m/px, ce qui éteignait aussi le grain
       sur les jeux de tuiles à 0,75 puis 0,50 : une plage de sable nu y devenait
       plus lisse en HR qu'en LR, faute de tout relief à montrer. La rampe part
       donc maintenant de 0,30 et n'atteint son plein qu'à 1 m/px.

       Le grain reste plein sur les fortes pentes, où il sert toujours, et sur
       les cartes larges restées grossières. Le dosage suit la finesse EFFECTIVE
       (tuiles comprises), sans quoi une carte large gardée grossière au loin
       poserait encore son tissage sur le sol fin juste sous l'appareil.
       u_orthoMPP vaut 0 si l'uniforme n'est pas fourni, ce qui éteint le
       plancher : repli sans danger. */
    float plancher = 0.15 * smoothstep(0.30, 1.00, finesse);
    float force  = mix(plancher, smoothstep(0.30, 1.00, finesse), pente)
                 * (1.0 - smoothstep(300.0, 1200.0, dist));
    vec3  albedo = ortho * mix(1.0, 0.72 + 0.56 * detail, force);

    /* Demi-Lambert : la lumière sculpte le relief sans plonger les versants à
       l'ombre dans le noir total. */
    float diffuse = dot(n, normalize(u_lightDir)) * 0.5 + 0.5;
    float light   = 0.55 + 0.55 * diffuse;
    vec3  color   = albedo * min(light, 1.3);

    /* La mer du terrain est rendue dans la couleur unie du plan de mer (même
       éclairage), partout où l'altitude est ~ 0. Cela donne une surface d'eau
       homogène, sans l'écume ni les sillages blancs de la photo satellite, et un
       raccord invisible avec le plan de mer au bord du bloc.
       Mais sur une côte basse (bassin d'Arcachon), des terres habitées sont aussi
       sous 3 m : la seule condition d'altitude leur donnait un sol bleu-vert. On
       ajoute donc un test de couleur sur l'orthophoto. L'eau y a deux signatures :
       un rouge faible (canal R ~ 0.1-0.2, contre R ~ 0.45+ pour les villes et le
       sable) ET un bleu nettement supérieur au rouge. Cette dominante bleue évite de
       teinter les sols sombres mais neutres (ombres, toits foncés) qu'un test sur le
       seul rouge prenait pour de l'eau. Mêmes conditions que le filtre des bâtiments
       sur l'eau (voir render/Buildings.cpp), pour que sol et bâtiments s'accordent. */
    float lowAlt  = 1.0 - smoothstep(0.0, 3.0, v_worldPos.y);   /* 1 près du niveau 0 */
    float redLow  = 1.0 - smoothstep(0.22, 0.40, ortho.r);      /* 1 si rouge faible */
    float blueDom = smoothstep(0.04, 0.12, ortho.b - ortho.r);  /* 1 si bleu domine (eau, pas une ombre) */
    float sea     = lowAlt * redLow * blueDom;                  /* eau = bas ET couleur d'eau */
    vec3  seaLit   = u_seaColor * min(light, 1.3);
    color          = mix(color, seaLit, sea);

    /* Brume : proportion croissante de couleur d'horizon avec la distance (dist
       déjà calculé en tête de main() pour le fondu du grain rocheux). */
    float fog  = smoothstep(u_fogStart, u_fogEnd, dist);
    color      = mix(color, u_fogColor, fog);

    frag_color = vec4(color, 1.0);
}
