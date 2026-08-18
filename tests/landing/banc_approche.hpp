/*
 * banc_approche.hpp
 * Banc d'essai des tests d'atterrissage automatique : simule une approche
 * complète et rend ce qu'on veut en mesurer.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/LandingAutopilot.hpp"
#include "app/LandingAutopilot.hpp"
#include "pas_simulation.hpp"
#include "physics/FlightModel.hpp"
#include "physics/constants.hpp"
#include "util/Math.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace essais_pose {

using artouste::vec3;
using artouste::app::LandingAutopilot;
using artouste::physics::Controls;
using artouste::physics::FlightModel;
using essais::SIM_DT;

using TerrainFn = std::function<float(float, float)>;

/* Écart (m) au-dessus duquel on considère l'appareil "en excédent notable" par
   rapport à la pente d'approche standard (GAIN_ALT_RETOUR * dist) -- sert à repérer
   une descente quasi immobile (vitesse sol proche de 0 alors qu'il reste beaucoup
   d'altitude à perdre), voir vitesseMinPendantExcesM. */
constexpr float SEUIL_EXCES_NOTABLE_M = 20.0f;

struct Resultat {
    bool  pose            = false;  /* landing.active() est devenu faux (posé) dans le temps imparti */
    float vitesseSolM     = 0.0f;   /* vitesse horizontale au posé (m/s) */
    float tauxDescenteM   = 0.0f;   /* |vitesse verticale| au posé (m/s) */
    float distancePadM    = 0.0f;   /* distance horizontale au centre du pad au posé (m) */
    float dureeS          = 0.0f;
    float clearanceMinM   = 0.0f;   /* plus petite hauteur-sol observée (hauteur - relief réel)
                                       en vol, avant la finale : négative si l'appareil est
                                       rentré dans le relief (CFIT). */
    float excesMaxM       = 0.0f;   /* plus grand écart observé entre agl et la pente standard
                                       (GAIN_ALT_RETOUR * dist), hors finale : mesure combien
                                       d'altitude "en trop" l'appareil a dû rattraper. */
    float vitesseSolMinPendantExcesM = -1.0f;  /* plus petite vitesse sol observée tant que
                                       l'excédent dépasse SEUIL_EXCES_NOTABLE_M (hors finale) ;
                                       -1 si l'excédent notable n'a jamais eu lieu. Proche de 0 :
                                       descente quasi immobile ("en stationnaire") signalée le
                                       16/07/2026, plutôt qu'un vol en avant qui perd l'altitude
                                       en trop tout en progressant vers le pad. */
};

/* Simule un atterrissage automatique complet, depuis un vol stationnaire à
   distanceInitialeM du pad (origine) et altitudeInitialeM de hauteur-sol, jusqu'au
   posé ou à dureeMaxS. Physique réaliste coupée pendant le vol automatique, comme le
   fait l'application (voir ApplicationLoop.cpp).
   terrain (optionnel) : relief entre l'appareil et le pad (sol plat à y = 0 si
   omis). Quand fourni, il est branché à la fois sur le contact sol de FlightModel
   (setGroundHeight, comme Application::mainLoop) et sur l'anticipation de relief de
   LandingAutopilot (hauteurMinRelief), pour rejouer fidèlement le cas réel. */
inline Resultat simulerApproche(float distanceInitialeM, float altitudeInitialeM, float dureeMaxS,
                         const TerrainFn& terrain = {}) {
    FlightModel model;
    model.reset(vec3{0.0f, altitudeInitialeM, distanceInitialeM}, 0.0f);
    model.turbine().forceRunning();
    model.setRealFlyPhysicsEnabled(false);

    /* Collectif de départ à la sustentation : comme en jeu (toggleAutoland passe
       m_lastControls, les commandes réelles du pilote au moment de l'engagement, pas
       des commandes neutres -- le pilote est toujours déjà en vol à ce moment-là). */
    Controls initial;
    initial.collective = artouste::physics::COLL_HOVER;

    LandingAutopilot landing;
    landing.start(vec3{0.0f, 0.0f, 0.0f}, initial);

    /* FlightModel a une vraie physique de contact : une fois posé (poussée sous le
       poids), la vitesse est remise à {0,0,0} d'un coup (voir FlightModel.cpp,
       "posé sur les patins"). Lire la vitesse une fois landing.active() devenu faux
       mesurerait donc cet état déjà calé, pas la vitesse d'impact réelle. On retient
       plutôt le pire taux de descente et la vitesse sol juste avant le contact
       (tant que l'appareil est encore en l'air, proche du sol). */
    float pireDescenteM = 0.0f;
    float vitesseAvantContactM = 0.0f;
    float clearanceMinM = 1.0e6f;
    float excesMaxM = 0.0f;
    float vitesseSolMinPendantExcesM = -1.0f;

    Resultat res;
    float t = 0.0f;
    while (t < dureeMaxS) {
        const vec3& pos     = model.body().position;
        const vec3& vel     = model.body().velocity;
        const vec3  fwd     = model.body().orientation * vec3{1.0f, 0.0f, 0.0f};
        const float heading = std::atan2(-fwd.z, fwd.x);
        const float sol     = terrain ? terrain(pos.x, pos.z) : 0.0f;
        const float agl     = pos.y - sol;
        if (terrain) {
            model.setGroundHeight(sol);
        }
        clearanceMinM = std::min(clearanceMinM, agl);

        if (agl > 0.0f && agl < 30.0f) {
            pireDescenteM        = std::max(pireDescenteM, -vel.y);
            vitesseAvantContactM = std::sqrt(vel.x * vel.x + vel.z * vel.z);
        }

        /* Écart à la pente standard, hors finale (dist < DIST_POSE, où l'écart
           résiduel est géré par une descente verticale contrôlée, pas un vol en
           avant -- voir LandingAutopilot::update). */
        using namespace artouste::app::demo_detail;
        const float dist = std::sqrt(pos.x * pos.x + pos.z * pos.z);
        if (dist > DIST_POSE) {
            const float hauteurPente = artouste::clamp(GAIN_ALT_RETOUR * dist, 0.0f, ALT_PLAFOND);
            const float exces        = agl - hauteurPente;
            if (exces > excesMaxM) {
                excesMaxM = exces;
            }
            if (exces > SEUIL_EXCES_NOTABLE_M) {
                const float vitesseSol = std::sqrt(vel.x * vel.x + vel.z * vel.z);
                if (vitesseSolMinPendantExcesM < 0.0f || vitesseSol < vitesseSolMinPendantExcesM) {
                    vitesseSolMinPendantExcesM = vitesseSol;
                }
            }
        }

        if (!landing.active()) {
            res.pose                         = true;
            res.vitesseSolM                  = vitesseAvantContactM;
            res.tauxDescenteM                = pireDescenteM;
            res.distancePadM                 = std::sqrt(pos.x * pos.x + pos.z * pos.z);
            res.dureeS                       = t;
            res.clearanceMinM                = clearanceMinM;
            res.excesMaxM                    = excesMaxM;
            res.vitesseSolMinPendantExcesM    = vitesseSolMinPendantExcesM;
            break;
        }
        const Controls controls = landing.update(SIM_DT, pos, vel, heading, agl, terrain);
        model.update(controls, SIM_DT);
        t += SIM_DT;
    }
    return res;
}

} /* namespace essais_pose */
