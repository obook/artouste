/*
 * landing_autopilot_tests.cpp
 * Test bout en bout de l'atterrissage automatique (LandingAutopilot) sur la
 * physique réaliste (FlightModel) : vérifie numériquement le taux de descente et
 * la vitesse à l'arrivée, plutôt que par le seul raisonnement -- plusieurs
 * régressions successives (dépassement du pad, atterrissage dur) ont montré les
 * limites du raisonnement seul sans données de trajectoire réelles.
 *
 * Scénario principal : approche Capbreton -> Hossegor (carte côte-landes), les
 * deux hélipads qui ont révélé le bug de dépassement à 95 km/h (~830 m entre eux).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/LandingAutopilot.hpp"
#include "physics/FlightModel.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using artouste::vec3;
using artouste::app::LandingAutopilot;
using artouste::physics::Controls;
using artouste::physics::FlightModel;

namespace {

constexpr float SIM_DT = 1.0f / 240.0f;

struct Resultat {
    bool  pose          = false;  /* landing.active() est devenu faux (posé) dans le temps imparti */
    float vitesseSolM   = 0.0f;   /* vitesse horizontale au posé (m/s) */
    float tauxDescenteM = 0.0f;   /* |vitesse verticale| au posé (m/s) */
    float distancePadM  = 0.0f;   /* distance horizontale au centre du pad au posé (m) */
    float dureeS        = 0.0f;
};

/* Simule un atterrissage automatique complet, depuis un vol stationnaire à
   distanceInitialeM du pad (origine, sol plat à y = 0) et altitudeInitialeM de
   hauteur-sol, jusqu'au posé ou à dureeMaxS. Physique réaliste coupée pendant le
   vol automatique, comme le fait l'application (voir ApplicationLoop.cpp). */
Resultat simulerApproche(float distanceInitialeM, float altitudeInitialeM, float dureeMaxS) {
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

    Resultat res;
    float t = 0.0f;
    while (t < dureeMaxS) {
        const vec3& pos     = model.body().position;
        const vec3& vel     = model.body().velocity;
        const vec3  fwd     = model.body().orientation * vec3{1.0f, 0.0f, 0.0f};
        const float heading = std::atan2(-fwd.z, fwd.x);
        const float agl     = pos.y;  /* sol plat à y = 0 */

        if (agl > 0.0f && agl < 30.0f) {
            pireDescenteM        = std::max(pireDescenteM, -vel.y);
            vitesseAvantContactM = std::sqrt(vel.x * vel.x + vel.z * vel.z);
        }

        if (!landing.active()) {
            res.pose          = true;
            res.vitesseSolM   = vitesseAvantContactM;
            res.tauxDescenteM = pireDescenteM;
            res.distancePadM  = std::sqrt(pos.x * pos.x + pos.z * pos.z);
            res.dureeS        = t;
            break;
        }
        const Controls controls = landing.update(SIM_DT, pos, vel, heading, agl);
        model.update(controls, SIM_DT);
        t += SIM_DT;
    }
    return res;
}

}  /* namespace anonyme */

TEST_CASE("Atterrissage automatique : approche longue Capbreton -> Hossegor (~830 m)", "[landing]") {
    const Resultat r = simulerApproche(830.0f, 150.0f, 180.0f);
    REQUIRE(r.pose);
    /* Seuil "atterrissage dur" réel pour un train à patins : ~2,4 à 3 m/s (voir la
       recherche du 2026-07). On vise nettement en dessous, comme le code (VZ_POSE). */
    CHECK(r.tauxDescenteM < 2.5f);
    /* Arrivée lente : pas le dépassement du pad à 95 km/h (~26 m/s) du bug d'origine. */
    CHECK(r.vitesseSolM < 3.0f);
    CHECK(r.distancePadM < 8.0f);   // sous le rayon réel du pad (PAD_PLATFORM_RADIUS_M, Hapi.hpp)
}

TEST_CASE("Atterrissage automatique : approche depuis une altitude excédentaire", "[landing]") {
    /* Même distance, mais parti nettement au-dessus de la pente à 6 % (0,06 * 830 =
       50 m visés, ici 250 m) : régime qui a révélé le plané-puis-chute, puis le
       dépassement du pad une fois le filet de vitesse mal calibré. */
    const Resultat r = simulerApproche(830.0f, 250.0f, 180.0f);
    REQUIRE(r.pose);
    CHECK(r.tauxDescenteM < 2.5f);
    CHECK(r.vitesseSolM < 3.0f);
    CHECK(r.distancePadM < 8.0f);   // sous le rayon réel du pad (PAD_PLATFORM_RADIUS_M, Hapi.hpp)
}

TEST_CASE("Atterrissage automatique : approche courte, déjà sur la pente", "[landing]") {
    /* Distance courte à l'altitude nominale de la pente (0,06 * 100 = 6 m) : le cas
       normal, sans excédent d'altitude à rattraper. */
    const Resultat r = simulerApproche(100.0f, 6.0f, 60.0f);
    REQUIRE(r.pose);
    CHECK(r.tauxDescenteM < 2.5f);
    CHECK(r.vitesseSolM < 3.0f);
    CHECK(r.distancePadM < 8.0f);   // sous le rayon réel du pad (PAD_PLATFORM_RADIUS_M, Hapi.hpp)
}
