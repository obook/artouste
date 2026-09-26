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

/* Liserets de diagnostic (clé "relief_debug") : largeur au sol des traits qui
   marquent les frontières de la fenêtre de relief, en mètres. Zéro les éteint.
   Le reste du terrain garde ses couleurs, ce qui rend le mode utilisable en vol
   réel. */
uniform float u_reliefLiseret;
uniform vec2  u_reliefDemiNoyau;   /* demi-côtés (x, z) du noyau */
uniform vec2  u_reliefDemiAnneau;  /* idem pour l'anneau */
uniform vec2  u_reliefCentre;      /* centre calé : il porte les deux contours */
uniform vec2  u_reliefOeil;        /* position continue : elle porte les cercles */
uniform vec2  u_reliefFondu;       /* début et fin du fondu, en distance à l'oeil */

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
/* Emprise de la carte et passe en cours, pour borner la fenêtre de relief (voir
   le début de main). Mêmes uniformes que terrain.vert, qui les pose déjà. */
uniform bool      u_reliefActif;
uniform vec2      u_carteCoin;    /* coin nord-ouest de l'emprise, monde */
uniform vec2      u_carteTailleM; /* emprise au sol (largeur, hauteur) */
uniform float     u_orthoMPP;   /* finesse de l'orthophoto (m au sol par pixel) */

/* Sonde de mise au point (ARTOUSTE_DEBUG_SONDE) : 3 écrit l'altitude du sol au
   lieu de sa couleur. À retirer. */
uniform int       u_sonde;

/* Vrai sur le CONTOUR d'un rectangle centré, à demi-largeur près. Un rectangle
   et non un carré : les deux pas de la grille diffèrent de quelques pour cent,
   et un contour carré manquerait la frontière de plusieurs dizaines de mètres
   sur une grille de deux kilomètres. */
bool surLeContour(vec2 ecart, vec2 demis, float demi) {
    return (abs(ecart.x - demis.x) < demi && ecart.y < demis.y + demi) ||
           (abs(ecart.y - demis.y) < demi && ecart.x < demis.x + demi);
}

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
float poserTuiles(sampler2D tuiles, sampler2D masque, vec2 monde, vec2 dmx, vec2 dmy,
                  vec2 ancre, float tailleM, float plein, float fondu, float dist,
                  inout vec3 couleur) {
    if (tailleM <= 0.0) {
        return 0.0;
    }
    vec2  uv   = (monde - ancre) / tailleM;
    vec2  dx   = dmx / tailleM;
    vec2  dy   = dmy / tailleM;
    float part = textureGrad(masque, uv, dx, dy).r * (1.0 - smoothstep(plein, fondu, dist));
    couleur    = mix(couleur, textureGrad(tuiles, uv, dx, dy).rgb, part);
    return part;
}

/* Couleur de la photo en un point du sol : orthophoto d'ensemble, puis les deux
   niveaux de tuiles par-dessus, du plus large au plus serré. Les dérivées écran
   (de l'UV de la carte et du point monde) sont passées au lieu d'être laissées
   au GPU : effacerTabliers lit la photo ailleurs que sous le fragment, dans une
   branche que tous les fragments ne prennent pas, où le GPU ne sait pas les
   calculer. Ce sont celles du fragment, la lecture n'étant que décalée. */
vec3 photo(vec2 uv, vec2 monde, vec2 duvx, vec2 duvy, vec2 dmx, vec2 dmy, float dist,
           out float fine, out float serre) {
    vec3 couleur = textureGrad(u_texture, uv, duvx, duvy).rgb;
    fine  = poserTuiles(u_fine, u_fineMasque, monde, dmx, dmy, u_fineAncre,
                        u_fineTailleM, u_finePlein, u_fineFondu, dist, couleur);
    serre = poserTuiles(u_serre, u_serreMasque, monde, dmx, dmy, u_serreAncre,
                        u_serreTailleM, u_serrePlein, u_serreFondu, dist, couleur);
    return couleur;
}

/* Ouvrages d'art proches de l'appareil (Buildings::tabliersProches). Le relief
   est le sol nu : sous un tablier, la photo montre encore le pont, peint au
   fond du lit. Vu de biais ou en passant dessous, on le voyait deux fois.

   On y remplace la photo par celle prise de part et d'autre du tablier, au
   même endroit le long de l'axe, fondue d'un bord à l'autre : le fleuve ou la
   route du dessous se prolonge. La passe des chaussées met u_nbTabliers à 0,
   le tablier garde donc la photo du pont. */
const int   TABLIERS_MAX    = 64;   /* MÊME valeur que TABLIERS_EFFACES_MAX (BuildingsGeometrie.hpp) */
/* La photo se lit à ce recul du bord effacé. À 1 m, on tombait sur les
   avant-becs des piles du Pont Neuf de Toulouse, qui débordent de 6 à 8 m, et
   sur l'ombre du pont : la bande effacée en sortait striée. */
const float TABLIER_RECUL_M = 6.0;
const float TABLIER_ETALEMENT_M = 3.0;  /* écart des lectures le long de l'axe */
const float TABLIER_PAS_M   = 2.0;  /* pas pour sortir d'un tablier voisin */
const int   TABLIER_PAS_MAX = 8;
uniform int   u_nbTabliers;
uniform vec4  u_tablier[TABLIERS_MAX];      /* axe : a.x, a.z, b.x, b.z (monde) */
/* Demi-largeurs effacées (m), à gauche (x) et à droite (y) de a vers b. */
uniform vec2  u_tablierDemi[TABLIERS_MAX];

/* Enfoncement du point p sous le tablier k, en mètres : distance au bord le plus
   proche, négative au-dehors. s reçoit l'écart signé à l'axe, positif à droite
   (la perpendiculaire (-ab.y, ab.x) pointe à droite dans ce repère, Z au sud). */
float enfoncement(int k, vec2 p, out float s) {
    vec2  a  = u_tablier[k].xy;
    vec2  ab = u_tablier[k].zw - a;
    float l2 = max(dot(ab, ab), 1e-6);
    float t  = dot(p - a, ab) / l2;
    s        = dot(p - a, vec2(-ab.y, ab.x)) * inversesqrt(l2);
    if (t < 0.0 || t > 1.0) {
        return -1.0;
    }
    return min(u_tablierDemi[k].y - s, u_tablierDemi[k].x + s);
}

/* Vrai si le point est sous l'un des tabliers reçus. */
bool sousUnTablier(vec2 p) {
    float s;
    for (int k = 0; k < u_nbTabliers; ++k) {
        if (enfoncement(k, p, s) > 0.0) {
            return true;
        }
    }
    return false;
}

/* Distance à l'axe, depuis q et dans la direction dir, du premier point hors de
   tout tablier. Un pont est souvent rendu en tronçons parallèles, chaussée et
   trottoir : lu juste au bord de l'un, on retombait sur l'autre, et c'était
   encore le pont qu'on recopiait sur l'eau. */
float horsDesTabliers(vec2 q, vec2 dir, float depart) {
    float d = depart;
    for (int essai = 0; essai < TABLIER_PAS_MAX && sousUnTablier(q + dir * d); ++essai) {
        d += TABLIER_PAS_M;
    }
    return d;
}

void effacerTabliers(vec2 uv, vec2 monde, vec2 duvx, vec2 duvy, vec2 dmx, vec2 dmy,
                     float dist, inout vec3 couleur, inout float fine, inout float serre) {
    /* Le tablier sous lequel le fragment est le plus enfoncé. Prendre le premier
       venu ne suffit pas : sous un pont en deux tronçons, le bord de l'un tombe
       au milieu de l'autre, et le fondu du bord y laissait revoir la photo du
       pont, marquage central compris. */
    int   k       = -1;
    float enfonce = 0.0;
    float s       = 0.0;
    for (int i = 0; i < u_nbTabliers; ++i) {
        float si;
        float e = enfoncement(i, monde, si);
        if (e > enfonce) {
            enfonce = e;
            k       = i;
            s       = si;
        }
    }
    if (k < 0) {
        return;
    }

    vec2 a = u_tablier[k].xy;
    vec2 u = normalize(u_tablier[k].zw - a);   /* le long de l'axe */
    vec2 n = vec2(-u.y, u.x);                  /* en travers, vers la droite */

    /* Pied du fragment sur l'axe, les deux points lus de part et d'autre (A à
       droite, B à gauche), puis le trajet du fragment à chacun. */
    vec2  q     = monde - n * s;
    float dA    = horsDesTabliers(q, n, u_tablierDemi[k].y + TABLIER_RECUL_M);
    float dB    = horsDesTabliers(q, -n, u_tablierDemi[k].x + TABLIER_RECUL_M);
    vec2  versA = n * (dA - s);
    vec2  versB = -n * (dB + s);

    /* Trois lectures de chaque côté, étalées le long de l'axe et moyennées :
       une seule recopiait en travers de la bande tout détail qui s'y trouvait
       (pile, ponton), en longues stries. En UV de carte, V est inversée : la
       rangée 0 est au nord. */
    vec3  cA = vec3(0.0), cB = vec3(0.0);
    float fineA = 0.0, serreA = 0.0, fineB = 0.0, serreB = 0.0;
    for (int e = -1; e <= 1; ++e) {
        vec2  vA = versA + u * (TABLIER_ETALEMENT_M * float(e));
        vec2  vB = versB + u * (TABLIER_ETALEMENT_M * float(e));
        float f1, s1, f2, s2;
        cA += photo(uv + vec2(vA.x, -vA.y) / u_carteTailleM, monde + vA,
                    duvx, duvy, dmx, dmy, dist, f1, s1);
        cB += photo(uv + vec2(vB.x, -vB.y) / u_carteTailleM, monde + vB,
                    duvx, duvy, dmx, dmy, dist, f2, s2);
        fineA += f1; serreA += s1; fineB += f2; serreB += s2;
    }

    float w = (s + dB) / (dA + dB);   /* 0 au point B, 1 au point A */
    /* Fondu sur le dernier mètre avant le bord : une coupure franche y traçait
       un liseré, la photo ne changeant pas d'un coup. */
    float f = smoothstep(0.0, 1.0, enfonce);
    couleur = mix(couleur, mix(cB, cA, w) / 3.0, f);
    fine    = mix(fine, mix(fineB, fineA, w) / 3.0, f);
    serre   = mix(serre, mix(serreB, serreA, w) / 3.0, f);
}

void main() {
    /* La grille de la fenêtre de relief est un carré centré sur l'appareil :
       près d'un bord de carte, elle DÉBORDE de l'emprise. Les textures de la
       carte étant en répétition, elle y dessinerait le relief et l'orthophoto
       de l'autre côté, ce qui se voit en vol comme une chute du sol puis une
       image répétée (constaté sur dax le 18/08/2026). On n'écrit donc rien
       au-delà du bord : un fragment écarté ne marque pas non plus le pochoir, et
       le maillage d'ensemble ne va pas plus loin, si bien qu'il reste le ciel,
       comme avant la fenêtre. Le maillage d'ensemble, lui, tient dans l'emprise
       et n'est pas concerné. */
    if (u_reliefActif) {
        vec2 dedans = (v_worldPos.xz + u_originXZ - u_carteCoin) / u_carteTailleM;
        if (any(lessThan(dedans, vec2(0.0))) || any(greaterThan(dedans, vec2(1.0)))) {
            discard;
        }
    }

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

    float dist  = length(u_camPos - v_worldPos);
    vec2  monde = v_worldPos.xz + u_originXZ;

    /* Dérivées écran pour toutes les lectures de la photo (voir photo), prises
       ici, avant tout retour anticipé : là, tous les fragments passent. */
    vec2 duvx = dFdx(v_uv);
    vec2 duvy = dFdy(v_uv);
    vec2 dmx  = dFdx(monde);
    vec2 dmy  = dFdy(monde);

    /* Liserets des frontières : le contour du noyau et celui de l'anneau autour
       du centre calé, les deux cercles du fondu autour de l'oeil. Tracés avant
       tout le reste, ils priment sur la couleur du sol. */
    if (u_reliefLiseret > 0.0) {
        vec2  ecart = abs(monde - u_reliefCentre);
        float rayon = distance(monde, u_reliefOeil);
        float demi  = 0.5 * u_reliefLiseret;
        if (surLeContour(ecart, u_reliefDemiNoyau, demi)) {
            frag_color = vec4(1.0, 0.0, 1.0, 1.0);   /* noyau : magenta */
            return;
        }
        if (surLeContour(ecart, u_reliefDemiAnneau, demi)) {
            frag_color = vec4(0.0, 1.0, 1.0, 1.0);   /* anneau : cyan */
            return;
        }
        if (abs(rayon - u_reliefFondu.x) < demi) {
            frag_color = vec4(1.0, 0.45, 0.0, 1.0);  /* début du fondu : orange */
            return;
        }
        if (abs(rayon - u_reliefFondu.y) < demi) {
            frag_color = vec4(1.0, 1.0, 0.0, 1.0);   /* fin du fondu : jaune */
            return;
        }
    }

    /* La photo sous ce fragment, tuiles comprises, débarrassée du pont qu'un
       tablier porte au-dessus. */
    float fine, serre;
    vec3  ortho = photo(v_uv, monde, duvx, duvy, dmx, dmy, dist, fine, serre);
    effacerTabliers(v_uv, monde, duvx, duvy, dmx, dmy, dist, ortho, fine, serre);

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
