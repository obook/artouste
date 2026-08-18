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

#include "app/demo/DemoNavigation.hpp"
#include "app/demo/DemoRelief.hpp"

#include "physics/constants.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app::demo_detail {

/* --- Découpage du début de la démo ------------------------------------------- */
inline constexpr float ROTOR_PRET      = 0.99f; /* régime rotor à atteindre avant de décoller (plein régime) */
inline constexpr float DELAI_DECOLLAGE = 3.0f;  /* s : attente sur le pad après le plein régime rotor avant de décoller */
inline constexpr float DUREE_MONTEE    = 9.0f;  /* s : décollage vertical doux avant de partir vers la dune */
inline constexpr float COLLECTIF_RATE  = 0.25f; /* 1/s : vitesse de variation du collectif (levier monté en douceur) */
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


}  /* namespace artouste::app::demo_detail */
