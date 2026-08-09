/*
 * DemoPilotDetail.hpp
 * Constantes de réglage et petites fonctions de guidage du pilote automatique de
 * démonstration (découpage des phases, cycle des vues, gains du guidage). Regroupés
 * ici en éléments "inline" (C++17) pour alléger DemoPilot.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/constants.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app::demo_detail {

/* --- Découpage du début de la démo ------------------------------------------- */
inline constexpr float ROTOR_PRET      = 0.99f; /* régime rotor à atteindre avant de décoller (plein régime) */
inline constexpr float DELAI_DECOLLAGE = 3.0f;  /* s : attente sur le pad après le plein régime rotor avant de décoller */
inline constexpr float DUREE_MONTEE    = 9.0f;  /* s : décollage vertical doux avant de partir vers la dune */
inline constexpr float COLLECTIF_RATE  = 0.25f; /* 1/s : vitesse de variation du collectif (levier monté en douceur) */
inline constexpr float VZ_DECOLLAGE      = 1.2f;  /* m/s : vitesse de montée visée au décollage (douce, contrôlée) */
inline constexpr float GAIN_VZ_DECOLLAGE = 0.06f; /* collectif par (m/s) d'écart à la vitesse de montée, au décollage */
inline constexpr float COLLECTIF_MAX   = 0.72f; /* plafond du collectif : garde la tuyère sous ~480 deg en montée
                                                   (temp = 400 + 150*collectif^2 ; 0,72 -> ~478 deg, zone verte) */
inline constexpr float DELAI_REDEMARRAGE = 5.0f;/* s : attente après l'arrêt des pales avant de relancer la démo */
inline constexpr float T_MAX        = 1200.0f; /* garde-fou : on relance la démo au plus tard à cet instant
                                                  (20 min) ; large marge pour la route courte Dune -> cap Ferret */

/* --- Cycle des vues en croisière (s) ----------------------------------------- */
inline constexpr float DUREE_POURSUITE     = 28.0f;  /* vue de poursuite (chase) */
inline constexpr float DUREE_COCKPIT       = 28.0f;  /* vue cockpit (intérieur) */
inline constexpr float DUREE_ORBITE        = 20.0f;  /* vue orbite (un tour complet, = DEMO_ORBIT_TURN côté application) */
inline constexpr float DUREE_ORBITE_SOLAIRE = 14.0f; /* vue orbite solaire (cadrage face au soleil, de jour) */
inline constexpr float CYCLE_VUES          = DUREE_POURSUITE + DUREE_COCKPIT + DUREE_ORBITE
                                           + DUREE_ORBITE_SOLAIRE;  /* durée d'un cycle complet */

/* --- Réglages du vol --------------------------------------------------------- */
inline constexpr float ALT_PLAFOND = 200.0f;  /* altitude de transit du retour et plafond de l'approche (m) :
                                                 évite de remonter haut depuis le dernier point bas (cap Ferret
                                                 en rase-mottes), pour une approche basse */
/* Vitesse de croisière visée : 170 km/h, la croisière documentée de la SE 313B
 * (voir REFERENCES.md et le tableau de docs/PROCEDURE_VOL.md). La valeur
 * précédente, 180 km/h, se plaçait à 97 % de la VNE : la démo volait alors voyant
 * de survitesse allumé, et depuis l'ajout du décrochage de pale reculante elle
 * aurait aussi traîné ses symptômes tout le long du parcours. */
inline constexpr float V_CROISIERE = 47.0f;   /* m/s, 170 km/h */
inline constexpr float RAYON_POINT = 300.0f;  /* distance à un point de passage en deçà de laquelle on vise le suivant (m) */

/* Plafond de vitesse pendant le retour au pad et l'atterrissage automatique,
 * distinct de V_CROISIERE (qui reste la vitesse de croisière entre points de
 * passage, hors approche). Une approche hélicoptère réelle s'amorce vers 70 kt
 * puis ralentit en continu (70, 65, 60, 50 kt...) jusqu'au posé ; avec le seul
 * V_CROISIERE (47 m/s, 170 km/h) comme plafond, l'appareil gardait une vitesse
 * quasi-croisière jusqu'à 357 m du pad (V_CROISIERE / GAIN_V_DIST) avant
 * d'entamer la décélération -- beaucoup plus tard que les 70 kt réels de
 * "début d'approche", d'où une approche perçue comme trop rapide. */
inline constexpr float V_APPROCHE_MAX = 36.0f;  /* m/s, 130 km/h (~70 kt) : plafond de vitesse en approche */

/* --- Gains du guidage proportionnel ------------------------------------------ */
inline constexpr float GAIN_CAP        = 1.4f;    /* palonnier par radian d'erreur de cap */
inline constexpr float CAP_MAX         = 0.7f;    /* palonnier maximal (évite de pivoter trop vite) */
inline constexpr float GAIN_ALT        = 0.020f;  /* collectif par mètre d'erreur d'altitude */
inline constexpr float GAIN_VZ         = 0.04f;   /* amortissement par la vitesse verticale (m/s) */
inline constexpr float COLL_ALT_CLAMP  = 0.30f;   /* borne basse du terme d'altitude sur le collectif :
                                                     limite la vitesse de descente (évite la chute vertigineuse
                                                     sur un grand écart, ex. 1000 m -> 30 m après la dune).
                                                     La montée, elle, n'est pas bornée. */
inline constexpr float GAIN_V_DIST     = 0.11f;   /* vitesse visée (m/s) par mètre de distance à la cible.
                                                     Descendu de 0,14 le 09/08/2026 : la cellule ayant
                                                     retrouvé sa traînée physique (KDRAG_FWD de 2,2 à 1,0),
                                                     elle ne freine plus autant toute seule et arrivait
                                                     au-dessus du pad encore trop haute, à descendre
                                                     presque sur place. Décélérer depuis plus loin
                                                     (327 m au lieu de 257 m sous le plafond de vitesse
                                                     d'approche) laisse le temps d'écouler l'excédent. */

/* Cyclique longitudinal (tangage) et latéral (roulis) : deux gains distincts,
 * car les deux axes n'ont plus la même autorité de couple depuis l'adoucissement
 * du roulis manuel (ROLL_CTRL très inférieur à PITCH_CTRL, voir constants.hpp).
 * Avec un seul gain isotrope, l'autopilote corrigeait le roulis bien plus
 * mollement que le tangage -- gênant en montagne (couloir étroit, col) où la
 * correction latérale doit pouvoir aller vite. Les valeurs latérales sont
 * remises à l'échelle du ratio PITCH_CTRL/ROLL_CTRL pour retrouver le même
 * budget de couple qu'avant ce changement. */
inline constexpr float GAIN_CYCLIQUE_LON = 0.08f;   /* cyclique par (m/s) d'écart de vitesse, tangage */
inline constexpr float CYCLIQUE_MAX_LON  = 0.45f;   /* cyclique maximal, tangage : assiette réaliste */
inline constexpr float GAIN_CYCLIQUE_LAT = 0.18f;   /* cyclique par (m/s) d'écart de vitesse, roulis */
inline constexpr float CYCLIQUE_MAX_LAT  = 1.00f;   /* cyclique maximal, roulis : pleine autorité disponible */
inline constexpr float GAIN_ALT_RETOUR = 0.06f;   /* pente d'approche du retour : hauteur visée (m) par mètre de
                                                     distance au pad. 6 % = même pente que le HAPI (slopePercent
                                                     usuel des balises, voir Hapi.hpp) : reste dans le secteur
                                                     "vert fixe" (OnSlope) plutôt que de survoler la pente réelle. */
inline constexpr float DIST_CAP_MIN    = 30.0f;   /* en deçà, on ne pivote plus le nez (la cible est trop proche) */
inline constexpr float VZ_POSE         = -0.8f;   /* m/s : vitesse de descente visée à la pose (douce mais sans traîner) */
inline constexpr float GAIN_VZ_POSE    = 0.15f;   /* collectif par (m/s) d'écart de vitesse verticale, à la pose */

/* --- Vues de la finale ------------------------------------------------------- */
inline constexpr float DIST_APPROCHE_FINALE = 250.0f;  /* au retour, en deçà : vue pilote (cockpit) pour l'approche */
inline constexpr float DIST_CABRAGE         = 40.0f;   /* au retour, en deçà : bascule en orbite pour le cabrage et la pose */

/* --- Détection de la pose ---------------------------------------------------- */
inline constexpr float DIST_POSE = 6.0f;   /* distance horizontale au pad sous laquelle on est "arrivé" (m) :
                                              sous le rayon de la plate-forme (PAD_PLATFORM_RADIUS_M = 8 m,
                                              Hapi.hpp), sans quoi "posé" peut se déclencher jusqu'à 7 m en
                                              dehors du pad -- l'appareil se pose alors à côté, pas dessus. */
inline constexpr float AGL_POSE  = 0.2f;   /* hauteur-sol sous laquelle on considère l'appareil vraiment posé (m) :
                                              on ne coupe la turbine qu'au contact, pour éviter une chute du dernier mètre */

/* Ramène un angle dans l'intervalle [-PI, +PI]. */
inline float wrapPi(float a) noexcept {
    a = std::fmod(a + PI, TWO_PI);
    if (a < 0.0f) {
        a += TWO_PI;
    }
    return a - PI;
}

/* Cap (rad) d'un vecteur monde horizontal, même convention que l'application :
   atan2(-z, x), donc 0 vers l'est, +PI/2 vers le nord. */
inline float bearing(float dx, float dz) noexcept {
    return std::atan2(-dz, dx);
}

/* Palonnier pour tourner le nez vers la cible (guidage en cap). Un cap visé plus à
   gauche (erreur positive) demande un palonnier négatif, car le palonnier droit
   (positif) fait partir le nez à droite et diminue le cap. */
inline float palonnierVers(const vec3& cible, const vec3& pos, float cap) noexcept {
    const float vise   = bearing(cible.x - pos.x, cible.z - pos.z);
    const float erreur = wrapPi(vise - cap);
    return clamp(-GAIN_CAP * erreur, -CAP_MAX, CAP_MAX);
}

/* Collectif pour rejoindre et tenir une hauteur-sol cible, amorti par la vitesse
   verticale pour ne pas osciller. Centré sur le collectif de sustentation. */
inline float collectifPour(float hauteurCible, float hauteurSol, float vitesseVerticale) noexcept {
    /* Terme d'altitude borné par le bas seulement : sans borne, un grand écart en
       descente (par ex. 1000 m -> 30 m après la dune) saturerait le collectif à zéro et
       l'appareil tomberait en chute libre. En bornant ce terme négatif, l'amortissement
       par la vitesse verticale garde la main et la descente se stabilise à une vitesse
       raisonnable. La montée, elle, garde toute son autorité (de toute façon plafonnée
       par COLLECTIF_MAX dans rampeCollectif). */
    float termeAlt = GAIN_ALT * (hauteurCible - hauteurSol);
    if (termeAlt < -COLL_ALT_CLAMP) {
        termeAlt = -COLL_ALT_CLAMP;
    }
    const float corr = termeAlt - GAIN_VZ * vitesseVerticale;
    return saturate(physics::COLL_HOVER + corr);
}

/* --- Approche en descente contrôlée (façon GPWS) ------------------------------ */
inline constexpr float GAIN_VZ_APPROCHE = 0.5f;  /* m/s de descente visée par mètre d'écart d'altitude */
inline constexpr float MARGE_GPWS       = 0.6f;  /* fraction du taux GPWS toléré effectivement visée :
                                                     une marge de sécurité, pas la limite pile.
                                                     Descendue de 0,7 à 0,6 le 09/08/2026 avec le
                                                     nouveau bilan de puissance : la traînée de cellule
                                                     ayant retrouvé sa valeur physique (KDRAG_FWD de 2,2
                                                     à 1,0), l'appareil freine moins tout seul et
                                                     l'approche arrivait à 2,74 m/s de taux de descente,
                                                     au-dessus du seuil de confort de 2,5. */

/* Collectif pour rejoindre hauteurCible en approche, avec un taux de descente
   plafonné sous le seuil de l'alerte GPWS du HUD (physics::gpwsSinkLimitMs, avec
   la marge MARGE_GPWS). Utilisé pour le retour au pad (DemoPilot) et l'atterrissage
   automatique (LandingAutopilot) à la place de collectifPour : celui-ci ne borne
   que le terme d'altitude du collectif, pas le taux de descente réellement obtenu,
   qui peut donc dépasser le seuil GPWS et déclencher l'alerte en approche. Ici, on
   vise directement une vitesse verticale (comme collectifPour au moment de la pose),
   dont la composante de descente est plafonnée à vzMaxDescente quel que soit l'écart
   d'altitude de départ. La montée, elle, n'est pas bornée, comme collectifPour. */
inline float collectifApprocheGpws(float hauteurCible, float hauteurSol,
                                   float vitesseVerticale) noexcept {
    const float vzMaxDescente = MARGE_GPWS * physics::gpwsSinkLimitMs(hauteurSol);
    const float ecart         = hauteurCible - hauteurSol;  /* > 0 : il faut monter */
    const float vzVoulue      = (ecart >= 0.0f) ? ecart * GAIN_VZ_APPROCHE
                                                 : std::max(ecart * GAIN_VZ_APPROCHE, -vzMaxDescente);
    const float corr          = GAIN_VZ_POSE * (vzVoulue - vitesseVerticale);
    return saturate(physics::COLL_HOVER + corr);
}

/* Filet de vitesse horizontale tant que l'appareil est plus haut que la pente
   d'approche (GAIN_ALT_RETOUR) ne l'exige à cette distance : sans lui, le
   ralentissement horizontal (piloté par la distance réelle) et la descente
   (plafonnée par collectifApprocheGpws) convergent à des rythmes indépendants ;
   l'appareil peut s'immobiliser au-dessus du pad avant d'avoir fini de perdre
   l'altitude en trop, et termine par une chute quasi verticale.

   Volontairement modeste (V_MIN_APPROCHE_HAUTE, quelques m/s) et non une remontée
   vers la croisière : une première version relançait la vitesse jusqu'à V_CROISIERE
   tant que trop haut, mais ne laissait alors que la finale (DIST_APPROCHE_FINALE à
   DIST_POSE) pour freiner jusqu'à zéro -- pas assez de distance pour décélérer
   depuis la croisière, d'où un dépassement du pad à grande vitesse. Un simple filet
   d'avance, lui, se dissipe sans jamais compromettre le freinage normal.

   S'estompe entre DIST_APPROCHE_FINALE (plein effet) et DIST_POSE (nul : la vitesse
   visée doit être nulle en finale, quelle que soit l'altitude résiduelle -- si
   l'appareil est encore trop haut à ce stade, mieux vaut une courte descente
   verticale contrôlée qu'un survol rapide du pad). */
inline constexpr float V_MIN_APPROCHE_HAUTE = 8.0f;  /* m/s : un filet d'avance, pas une croisière */

inline float vitesseMinApproche(float dist, float aglSurPad) noexcept {
    if (aglSurPad <= GAIN_ALT_RETOUR * dist || dist <= DIST_POSE) {
        return 0.0f;  /* pas trop haut, ou déjà en finale : le freinage normal suffit */
    }
    if (dist >= DIST_APPROCHE_FINALE) {
        return V_MIN_APPROCHE_HAUTE;
    }
    const float t = (dist - DIST_POSE) / (DIST_APPROCHE_FINALE - DIST_POSE);  /* 0 -> 1 */
    return t * V_MIN_APPROCHE_HAUTE;
}

/* --- Relief intermédiaire (col, flanc de montagne) ---------------------------- */
inline constexpr float PAS_SONDE_RELIEF = 25.0f;  /* m : intervalle entre deux sondes sur la
                                                      route directe vers la cible. Fixe plutôt
                                                      que proportionnel à la distance : sur une
                                                      longue approche, un pas trop large sauterait
                                                      par-dessus une arête étroite. */
inline constexpr float MARGE_RELIEF     = 20.0f;  /* m : franchise visée au-dessus du point de
                                                      relief le plus haut trouvé sur la route. */
inline constexpr float PENTE_MONTEE_RELIEF = 0.25f;  /* pente de montée anticipée (m par m de
                                                      distance au point sondé), pour ne viser la
                                                      hauteur d'un relief qu'à mesure qu'il se
                                                      rapproche, pas dès qu'il entre dans
                                                      l'horizon sondé (jusqu'à dist, potentiellement
                                                      des centaines de mètres) -- plus raide que la
                                                      pente de descente (GAIN_ALT_RETOUR, 6 %) car
                                                      grimper n'est pas plafonné par le GPWS comme
                                                      la descente (collectifApprocheGpws). Sans
                                                      cette pente, un relief encore loin (donc pas
                                                      urgent) faisait grimper l'appareil aussitôt à
                                                      sa pleine hauteur, qu'il tenait alors sur tout
                                                      le reste du trajet jusqu'à lui : l'excédent à
                                                      perdre ensuite, une fois le relief dépassé,
                                                      pouvait dépasser ce que vitesseMinApproche
                                                      (filet de vitesse, tuné pour un excédent
                                                      modéré) suffit à écouler avant la finale,
                                                      d'où une descente quasi immobile sur 100 à
                                                      300 m (signalé le 16/07/2026). */

/* Hauteur-sol minimale (même sens que hauteurCible : au-dessus du point courant) à
   tenir pour survoler avec MARGE_RELIEF le relief le plus haut rencontré entre
   position et cible, en anticipant chaque point sondé avec une pente de montée
   PENTE_MONTEE_RELIEF plutôt qu'en visant sa hauteur immédiatement. hauteurCible
   normale (GAIN_ALT_RETOUR * dist) suppose une pente de terrain régulière jusqu'au
   pad ; sur un col ou un flanc de montagne entre les deux, la ligne directe peut
   couper un point plus haut que le pad avant de l'atteindre. Sans anticipation, le
   pilote automatique ne réagit qu'à agl (la hauteur-sol sous l'appareil À L'INSTANT
   présent, voir collectifApprocheGpws) : le relief surgit alors trop tard pour que
   le collectif (lissé par rampeCollectif) ait le temps de faire grimper l'appareil,
   d'où un CFIT (vol contrôlé vers le terrain) plutôt qu'un problème de puissance.
   Recalculée à chaque appel depuis la position courante (horizon fuyant) : un
   relief déjà franchi sort naturellement de l'intervalle sondé, un relief encore à
   venir y reste tant qu'il n'a pas été dépassé.
   terrainHeight(x, z) renvoie l'altitude du sol à ce point (même fonction que
   render::Terrain::heightAt). Renvoie 0 si aucun relief intermédiaire n'impose de
   monter plus haut que hauteurCible ne le ferait déjà (cas courant, terrain plat ou
   en pente régulière jusqu'au pad). */
template <typename TerrainHeightFn>
inline float hauteurMinRelief(const vec3& position, const vec3& cible, float dist,
                              TerrainHeightFn&& terrainHeight) noexcept {
    if (dist <= PAS_SONDE_RELIEF) {
        return 0.0f;  /* trop près pour qu'un relief intermédiaire ait un sens */
    }
    const float solPosition = terrainHeight(position.x, position.z);
    float       hauteurMax  = 0.0f;
    for (float d = PAS_SONDE_RELIEF; d < dist; d += PAS_SONDE_RELIEF) {
        const float t   = d / dist;
        const float x   = position.x + (cible.x - position.x) * t;
        const float z   = position.z + (cible.z - position.z) * t;
        const float msl = terrainHeight(x, z) + MARGE_RELIEF;
        /* Hauteur-sol qu'il faut tenir MAINTENANT pour atteindre msl au point sondé
           en grimpant à PENTE_MONTEE_RELIEF -- pas la hauteur du relief elle-même :
           un point encore loin (d grand) n'exige donc pas de monter tout de suite. */
        const float hauteurSol = msl - solPosition - PENTE_MONTEE_RELIEF * d;
        if (hauteurSol > hauteurMax) {
            hauteurMax = hauteurSol;
        }
    }
    return hauteurMax > 0.0f ? hauteurMax : 0.0f;
}

/* Collectif de décollage : au lieu de tirer le levier à fond (cap à COLLECTIF_MAX) en
   visant l'altitude de survol, on asservit la VITESSE DE MONTÉE à une valeur douce
   (VZ_DECOLLAGE). Le collectif reste donc juste au-dessus de la sustentation et
   l'appareil quitte le pad lentement, sans à-coup. La rampe du levier (rampeCollectif)
   lisse encore l'instant initial. */
inline float collectifDecollage(float vitesseVerticale) noexcept {
    const float corr = GAIN_VZ_DECOLLAGE * (VZ_DECOLLAGE - vitesseVerticale);
    return saturate(physics::COLL_HOVER + corr);
}

}  /* namespace artouste::app::demo_detail */
