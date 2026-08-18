/*
 * flight_model_sustentation_tests.cpp
 * Sustentation, effet de sol, translation, plafonds transitoires et TMP.
 *
 * Tournent sans contexte graphique. Outils communs dans aide_vol.hpp.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "flight_model/aide_vol.hpp"

#include "physics/constants.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using artouste::physics::Controls;
using artouste::physics::FlightModel;
using essais_vol::advance;
using essais_vol::SIM_DT;
using essais_vol::tiltAngle;

TEST_CASE("Hors effet de sol, le collectif de sustentation tient l'altitude", "[flight]") {
    FlightModel model;
    model.reset(50.0f);  /* assez haut pour ignorer l'effet de sol */
    model.turbine().forceRunning();  /* rotor en régime, sinon pas de portance */
    Controls hover;
    /* La densité de l'air diminue avec l'altitude : à 50 m il faut un peu plus de
     * collectif que COLL_HOVER (calé au niveau de la mer) pour équilibrer le poids. */
    const float densite = std::exp(-50.0f / artouste::physics::AIR_DENSITY_SCALE);
    hover.collective = artouste::physics::COLL_HOVER / densite;
    advance(model, hover, 5.0f);

    /* L'appareil ne doit ni s'enfoncer ni grimper de façon notable. */
    REQUIRE(std::fabs(model.body().position.y - 50.0f) < 1.0f);
    REQUIRE(std::fabs(model.body().velocity.y) < 1.0f);
}

TEST_CASE("En altitude, la température tuyère suit la puissance, pas le levier", "[flight]") {
    FlightModel model;
    model.reset(2884.0f);  /* altitude du pic du Midi d'Ossau */
    model.turbine().forceRunning();
    Controls hover;
    /* Collectif de stationnaire à cette altitude : le levier est très haut (~93 %)
     * mais la puissance produite reste celle d'un stationnaire. La tuyère doit donc
     * rester sous le seuil orange : le levier plus haut en altitude ne doit pas
     * pénaliser deux fois la température. */
    const float densite = std::exp(-2884.0f / artouste::physics::AIR_DENSITY_SCALE);
    hover.collective = artouste::physics::COLL_HOVER / densite;
    advance(model, hover, 60.0f);

    REQUIRE(model.turbine().exhaustTempC() < artouste::physics::EXHAUST_TEMP_WARN_C);
}

TEST_CASE("En mode assisté, l'altitude ne pénalise plus la sustentation", "[flight]") {
    FlightModel model;
    model.reset(50.0f);
    model.turbine().forceRunning();
    model.setRealFlyPhysicsEnabled(false);  /* difficultés coupées, comme en mode assisté */
    Controls hover;
    /* Sans pénalité de densité, le collectif de sustentation suffit à n'importe
     * quelle altitude, comme avant l'ajout des difficultés. */
    hover.collective = artouste::physics::COLL_HOVER;
    advance(model, hover, 5.0f);

    REQUIRE(std::fabs(model.body().position.y - 50.0f) < 1.0f);
    REQUIRE(std::fabs(model.body().velocity.y) < 1.0f);
}

TEST_CASE("L'effet de sol soulève au ras du sol", "[flight][m5]") {
    FlightModel model;  /* au sol (y = 0) */
    model.turbine().forceRunning();
    Controls    hover;
    hover.collective = artouste::physics::COLL_HOVER;
    advance(model, hover, 4.0f);

    /* Au même collectif de sustentation, le coussin d'air fait décoller. */
    REQUIRE(model.body().position.y > 0.5f);
}

TEST_CASE("L'effet de translation augmente la poussée", "[flight][m5]") {
    FlightModel model;
    model.reset(50.0f);
    model.turbine().forceRunning();
    Controls hover;
    hover.collective = artouste::physics::COLL_HOVER;
    advance(model, hover, 2.0f);
    const float hoverThrust = model.lastThrust();

    /* On prend de la vitesse vers l'avant. */
    Controls forward = hover;
    forward.cyclicLongitudinal = 1.0f;
    advance(model, forward, 6.0f);

    const auto& v             = model.body().velocity;
    const float airspeed      = std::sqrt(v.x * v.x + v.z * v.z);
    REQUIRE(airspeed > artouste::physics::ETL_V_HIGH);
    REQUIRE(model.lastThrust() > hoverThrust * 1.02f);
}

TEST_CASE("Le plancher transitoire tient le stationnaire à 3500 m", "[flight][transitoire]") {
    /* Au régime continu (200 kW x densité), le stationnaire s'arrête vers 3090 m.
       Le plancher transitoire (POWER_FLAT_W, décision du 11/08/2026, manuel page 10 :
       plafond HES 4070 m à 1100 kg) doit le porter au-delà, plein levier. */
    FlightModel model;
    model.reset(3500.0f);
    model.turbine().forceRunning();
    Controls plein;
    plein.collective = 1.0f;
    advance(model, plein, 60.0f);

    REQUIRE(model.body().position.y > 3470.0f);           /* il ne s'enfonce pas */
    REQUIRE(model.turbine().exhaustTempC() > 440.0f);     /* et la tuyère chauffe */
}

TEST_CASE("Le plafond transitoire tient encore à 4000 m", "[flight][transitoire]") {
    /* Le point qui garde le calage : le plafond de stationnaire HES du manuel
       (4070 m à 1100 kg) se prend plein levier, et POWER_FLAT_W est calé pour
       que l'excédent y soit encore franchement positif. Ce test échouerait si
       l'absorption cessait de s'appliquer au plancher, ou si le plancher était
       recalé sans revoir le pas du plafond. */
    FlightModel model;
    model.reset(4000.0f);
    model.turbine().forceRunning();
    Controls plein;
    plein.collective = 1.0f;
    advance(model, plein, 120.0f);

    REQUIRE(model.body().position.y > 4000.0f);           /* il monte encore */
    REQUIRE(model.body().velocity.y > 0.0f);
    REQUIRE(model.surchauffe() > 0.2f);                   /* le transitoire est sollicité */
}

TEST_CASE("La surchauffe ne court pas en physique assistée", "[flight][transitoire]") {
    /* Le bilan de puissance est débranché en assisté, en démo et pendant
       l'atterrissage automatique : la surchauffe du transitoire n'y a aucun sens
       et ne doit pas allumer l'alarme TMP en altitude. */
    FlightModel model;
    model.reset(4000.0f);
    model.turbine().forceRunning();
    model.setRealFlyPhysicsEnabled(false);
    Controls plein;
    plein.collective = 1.0f;
    advance(model, plein, 120.0f);

    REQUIRE(model.surchauffe() < 0.05f);
    REQUIRE(model.turbine().exhaustTempC() < artouste::physics::EXHAUST_TEMP_MAXI_C);
}

TEST_CASE("La TMP suit la loi pas/température, pas le tout-ou-rien", "[flight][tmp]") {
    /* Plein levier au niveau de la mer : la loi de la planche Abb. 2-23 donne
       environ 488 degrés à 15 degrés de pas en ISA, plutôt que le vieux 550
       systématique du modèle charge au carré. */
    FlightModel model;
    model.reset(50.0f);
    model.turbine().forceRunning();
    Controls plein;
    plein.collective = 1.0f;
    advance(model, plein, 60.0f);

    REQUIRE(model.turbine().exhaustTempC() > 460.0f);
    REQUIRE(model.turbine().exhaustTempC() < artouste::physics::EXHAUST_TEMP_MAXI_C);
}

TEST_CASE("La zone hauteur-vitesse s'allume en stationnaire haut", "[flight][hv]") {
    /* Stationnaire à 60 m sol sans vitesse : en plein dans la zone à éviter du
       diagramme hauteur-vitesse (autorotation non garantie). Turbine coupée,
       l'indicateur doit retomber : on est déjà en autorotation. */
    FlightModel model;
    model.reset(60.0f);
    model.turbine().forceRunning();
    Controls hover;
    hover.collective = artouste::physics::COLL_HOVER /
                       std::exp(-60.0f / artouste::physics::AIR_DENSITY_SCALE);
    advance(model, hover, 8.0f);
    REQUIRE(model.hvIntensity() > 0.5f);

    model.turbine().stopNow();
    advance(model, hover, 8.0f);
    REQUIRE(model.hvIntensity() < 0.2f);
}
