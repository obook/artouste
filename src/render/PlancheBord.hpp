/*
 * PlancheBord.hpp
 * Cadrans dessinés de la planche du poste 3dexport, d'après le manuel de vol
 * SE 3130 (figure 2-5a, graduations de l'appendice 1) : l'état que lisent
 * leurs aiguilles, et les échelles qui changent une valeur en angle.
 *
 * Les faces sont dessinées dans l'atlas intérieur par
 * tools/modele/interieur_4096.py avec les mêmes échelles : changer l'une,
 * c'est changer l'autre.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

namespace artouste::render {

/* Ce que montrent les cadrans dessinés, rempli à chaque image par
   l'application (voir Application::renderScene). */
struct EtatPlanche {
    float heure      = 10.0f;   /* heure du jeu, en heures décimales */
    float turbineTpm = 0.0f;    /* régime turbine, tr/min */
    float rotorTpm   = 0.0f;    /* régime rotor, tr/min */
    float tuyereC    = 15.0f;   /* température tuyère (T4), °C */
    float pasDeg     = 0.0f;    /* pas collectif, degrés de pale */
    float carburantL = 0.0f;    /* carburant, litres */
    float volts      = 24.0f;   /* tension du réseau de bord */
    float huileC     = 15.0f;   /* température d'huile moteur, °C */
    float huileBar   = 0.0f;    /* pression d'huile moteur, bar */
    float servoBar   = 0.0f;    /* pression des servocommandes, bar */
    float capDeg     = 0.0f;    /* cap, degrés : 0 au nord, 90 à l'est */
    float virageDegS = 0.0f;    /* taux de virage, deg/s, positif à droite */
    float billeG     = 0.0f;    /* bille : force latérale en g, positif à droite */
    float airC       = 15.0f;   /* température extérieure, °C */
    /* Voyants de la console, sous la planche (repères du manuel). */
    bool voyantRelais      = false;  /* relais démarreur (17), rouge */
    bool voyantDemarreur   = false;  /* démarreur en marche (19), vert */
    bool voyantMicropompe  = false;  /* micropompe carburant (20), ambre */
    bool voyantGeneratrice = false;  /* génératrice hors ligne (48), rouge */
    bool voyantHuileBtp    = false;  /* pression huile boîte de transmission (6), rouge */
};

/* Échelle d'un cadran : angle de la valeur 'vmin' en degrés depuis midi,
   sens horaire vu du pilote, étendue jusqu'à 'vmax' (négative : sens
   inverse). */
struct Echelle {
    float debut;
    float etendue;
    float vmin;
    float vmax;
};

/* Angle de l'aiguille pour la valeur 'v', en butée hors de l'échelle. */
inline float angleSur(const Echelle& e, float v) {
    const float t = glm::clamp((v - e.vmin) / (e.vmax - e.vmin), 0.0f, 1.0f);
    return e.debut + e.etendue * t;
}

inline constexpr Echelle ECHELLE_TURBINE{-135.0f, 270.0f, 0.0f, 40000.0f};  /* tr/min */
inline constexpr Echelle ECHELLE_ROTOR{-135.0f, 270.0f, 0.0f, 420.0f};      /* tr/min */
/* Unité triple : tuyère sur l'arc du haut, autour du centre ; huile sur deux
   petits arcs autour d'un pivot bas, température à gauche, pression à droite. */
inline constexpr Echelle ECHELLE_TUYERE{-65.0f, 130.0f, 0.0f, 800.0f};      /* °C */
inline constexpr Echelle ECHELLE_HUILE_C{-100.0f, 85.0f, 0.0f, 120.0f};     /* °C */
inline constexpr Echelle ECHELLE_HUILE_BAR{100.0f, -85.0f, 0.0f, 0.8f};     /* bar */
inline constexpr Echelle ECHELLE_PAS{-75.0f, 150.0f, 0.0f, 20.0f};          /* degrés */
inline constexpr Echelle ECHELLE_CARBURANT{-60.0f, 120.0f, 0.0f, 150.0f};   /* gallons US */
inline constexpr Echelle ECHELLE_VOLTS{-60.0f, 120.0f, 0.0f, 30.0f};        /* volts */
inline constexpr Echelle ECHELLE_SERVO{-135.0f, 270.0f, 0.0f, 40.0f};       /* bar */
/* Côté copilote, hors figure du manuel : bille-aiguille, repère du taux
   standard (3 deg/s) à 25 deg ; thermomètre extérieur. */
inline constexpr Echelle ECHELLE_VIRAGE{-50.0f, 100.0f, -6.0f, 6.0f};       /* deg/s */
inline constexpr Echelle ECHELLE_AIR{-135.0f, 270.0f, -30.0f, 50.0f};       /* °C */

/* Température de l'air à l'altitude 'altitudeM' (mètres), atmosphère type :
   15 °C au niveau de la mer, -6,5 °C par 1 000 m. La météo n'est pas simulée. */
inline float temperatureAir(float altitudeM) {
    return 15.0f - 0.0065f * altitudeM;
}

/* Le cadran carburant est gradué en gallons US (x 10), le simulateur compte
   en litres. */
inline float gallonsUs(float litres) {
    return litres / 3.785f;
}

}  /* namespace artouste::render */
