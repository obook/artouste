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
inline constexpr float DUREE_MONTEE    = 5.0f;  /* s : décollage vertical avant de partir vers la dune */
inline constexpr float COLLECTIF_RATE  = 0.35f; /* 1/s : vitesse de variation du collectif (levier monté en douceur) */
inline constexpr float COLLECTIF_MAX   = 0.72f; /* plafond du collectif : garde la tuyère sous ~480 deg en montée
                                                   (temp = 400 + 150*collectif^2 ; 0,72 -> ~478 deg, zone verte) */
inline constexpr float DELAI_REDEMARRAGE = 5.0f;/* s : attente après l'arrêt des pales avant de relancer la démo */
inline constexpr float T_MAX        = 720.0f; /* garde-fou : on relance la démo au plus tard à cet instant (12 min) */

/* --- Cycle des vues en croisière (s) ----------------------------------------- */
inline constexpr float DUREE_POURSUITE = 28.0f;  /* vue de poursuite (chase) */
inline constexpr float DUREE_COCKPIT   = 28.0f;  /* vue cockpit (intérieur) */
inline constexpr float DUREE_ORBITE    = 14.0f;  /* vue orbite (un tour complet, = DEMO_ORBIT_TURN côté application) */
inline constexpr float CYCLE_VUES      = DUREE_POURSUITE + DUREE_COCKPIT + DUREE_ORBITE;  /* durée d'un cycle complet */

/* --- Réglages du vol --------------------------------------------------------- */
inline constexpr float ALT_SURVOL  = 1500.0f; /* hauteur de survol de la Dune du Pilat (m) */
inline constexpr float V_CROISIERE = 35.0f;   /* vitesse de croisière visée (m/s) : assiette de croisière réaliste (~10 deg) */
inline constexpr float RAYON_DUNE  = 300.0f;  /* distance à la dune en deçà de laquelle on entame le demi-tour (m) */

/* --- Gains du guidage proportionnel ------------------------------------------ */
inline constexpr float GAIN_CAP        = 1.4f;    /* palonnier par radian d'erreur de cap */
inline constexpr float CAP_MAX         = 0.7f;    /* palonnier maximal (évite de pivoter trop vite) */
inline constexpr float GAIN_ALT        = 0.020f;  /* collectif par mètre d'erreur d'altitude */
inline constexpr float GAIN_VZ         = 0.04f;   /* amortissement par la vitesse verticale (m/s) */
inline constexpr float GAIN_V_DIST     = 0.14f;   /* vitesse visée (m/s) par mètre de distance à la cible */
inline constexpr float GAIN_CYCLIQUE   = 0.08f;   /* cyclique par (m/s) d'écart de vitesse */
inline constexpr float CYCLIQUE_MAX    = 0.45f;   /* cyclique maximal : borne l'inclinaison à une assiette réaliste */
inline constexpr float GAIN_ALT_RETOUR = 0.20f;   /* hauteur visée (m) par mètre de distance au pad (descente du retour) */
inline constexpr float DIST_CAP_MIN    = 30.0f;   /* en deçà, on ne pivote plus le nez (la cible est trop proche) */
inline constexpr float VZ_POSE         = -0.8f;   /* m/s : vitesse de descente visée à la pose (douce mais sans traîner) */
inline constexpr float GAIN_VZ_POSE    = 0.15f;   /* collectif par (m/s) d'écart de vitesse verticale, à la pose */

/* --- Détection de la pose ---------------------------------------------------- */
inline constexpr float DIST_POSE = 15.0f;  /* distance horizontale au pad sous laquelle on est "arrivé" (m) */
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
    const float corr = GAIN_ALT * (hauteurCible - hauteurSol) - GAIN_VZ * vitesseVerticale;
    return saturate(physics::COLL_HOVER + corr);
}

}  /* namespace artouste::app::demo_detail */
