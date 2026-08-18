/*
 * flight_model_turbine_tests.cpp
 * Turbine et carburant : refroidissement, mise en régime, pannes sèches.
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

TEST_CASE("La tuyère refroidit en deux temps après la coupure", "[flight][turbine][tmp]") {
    /* Une turbine ne revient pas à l'air ambiant en une demi-minute : la flamme
       s'éteint d'un coup, la sonde décroche en quelques secondes, puis elle suit
       la chaleur du métal qui l'entoure et met des minutes à se vider. On pilote
       la turbine seule, sans modèle de vol : c'est elle qui porte la thermique. */
    using artouste::physics::Turbine;
    namespace phys = artouste::physics;

    Turbine turbine;
    turbine.forceRunning();
    const auto chauffe = [&turbine](float secondes, float cible) {
        for (int i = 0; i < static_cast<int>(secondes / SIM_DT); ++i) {
            turbine.update(SIM_DT, cible);
        }
    };
    chauffe(60.0f, 450.0f);
    REQUIRE(turbine.exhaustTempC() > 440.0f);  /* tuyère chaude, régime établi */

    turbine.toggle();  /* coupure */
    REQUIRE(turbine.state() == Turbine::State::Extinction);

    /* Chute franche : dix secondes suffisent à quitter les 450 degrés... */
    chauffe(10.0f, 450.0f);
    REQUIRE(turbine.exhaustTempC() < 300.0f);
    /* ... mais la queue de chaleur résiduelle tient la sonde bien au-dessus de
       l'ambiante une minute plus tard, là où l'ancien modèle l'y ramenait. */
    chauffe(20.0f, 450.0f);
    REQUIRE(turbine.exhaustTempC() < 300.0f);   /* t + 30 s */
    chauffe(30.0f, 450.0f);
    const float apresUneMinute = turbine.exhaustTempC();
    REQUIRE(apresUneMinute > 150.0f);           /* t + 60 s */

    /* Redémarrage en cours de refroidissement : la tuyère repart de sa
       température courante, elle ne se téléporte ni au chaud ni au froid. */
    turbine.toggle();
    REQUIRE(turbine.state() == Turbine::State::Demarrage);
    chauffe(0.5f, 450.0f);
    REQUIRE(turbine.exhaustTempC() > apresUneMinute - 20.0f);

    /* stopNow reste une remise à zéro d'état (tests, mode capture) : froid net. */
    turbine.stopNow();
    turbine.update(SIM_DT, 450.0f);
    REQUIRE(turbine.exhaustTempC() < phys::EXHAUST_TEMP_AMBIENT_C + 1.0f);
}

TEST_CASE("Turbine coupée, plein collectif ne décolle pas", "[flight][turbine]") {
    FlightModel model;  /* turbine à l'arrêt par défaut au lancement */
    REQUIRE(model.turbine().state() == artouste::physics::Turbine::State::Arret);

    Controls climb;
    climb.collective = 1.0f;
    advance(model, climb, 3.0f);

    /* Sans rotor entraîné, aucune portance : l'appareil reste posé. */
    REQUIRE(model.body().position.y < 0.001f);
}

TEST_CASE("Le rotor attend le plein régime de la turbine", "[flight][turbine]") {
    using State = artouste::physics::Turbine::State;
    FlightModel    model;
    const Controls idle;  /* on n'observe que la turbine */

    /* Démarrage : la turbine monte d'abord seule, rotor encore immobile. */
    model.turbine().toggle();
    REQUIRE(model.turbine().state() == State::Demarrage);
    advance(model, idle, artouste::physics::TURBINE_START_TIME * 0.5f);
    REQUIRE(model.turbine().state() == State::Demarrage);
    REQUIRE(model.turbine().rotorFraction() == 0.0f);  /* pales encore à l'arrêt */
    REQUIRE(model.turbine().turbineFraction() > 0.0f); /* turbine en train de monter */

    /* La turbine atteint son plein régime, mais le frein rotor est encore serré :
       les pales restent immobiles le temps que le pilote le lâche (état Attente).
       On s'arrête au milieu du délai de frein pour observer cet état, quelles que
       soient les durées choisies pour la montée et le délai. */
    advance(model, idle,
            artouste::physics::TURBINE_START_TIME * 0.5f +
                artouste::physics::ROTOR_BRAKE_DELAY * 0.5f);
    REQUIRE(model.turbine().state() == State::Attente);
    REQUIRE(model.turbine().turbineFraction() == 1.0f);
    REQUIRE(model.turbine().rotorFraction() == 0.0f);  /* pales toujours à l'arrêt */

    /* Frein lâché après le délai : le rotor s'accouple puis atteint son régime. */
    advance(model, idle, artouste::physics::ROTOR_BRAKE_DELAY);
    REQUIRE(model.turbine().state() == State::Embrayage);
    advance(model, idle, artouste::physics::ROTOR_ENGAGE_TIME + 1.0f);
    REQUIRE(model.turbine().state() == State::Regime);
    REQUIRE(model.turbine().rotorFraction() == 1.0f);

    /* Arrêt : turbine et rotor redescendent (le rotor, plus lent, donne le tempo).
       On attend la somme des deux temps pour être sûr que tout est immobile. */
    model.turbine().toggle();
    REQUIRE(model.turbine().state() == State::Extinction);
    advance(model, idle,
            artouste::physics::ROTOR_STOP_TIME + artouste::physics::TURBINE_STOP_TIME + 1.0f);
    REQUIRE(model.turbine().state() == State::Arret);
    REQUIRE(model.turbine().rotorFraction() == 0.0f);
}

TEST_CASE("le reset avec cap oriente l'appareil à la boussole", "[flight]") {
    /* Convention : l'axe avant du corps est +X, le nord est -Z. Un cap de 90
       (est) doit donc laisser l'orientation à l'identité, 180 pointer vers +Z
       (sud) et 0 vers -Z (nord). Voir Terrain::startHeadingDeg. */
    FlightModel model;
    const artouste::vec3 pos{0.0f, 100.0f, 0.0f};

    model.reset(pos, 90.0f);
    artouste::vec3 avant = model.body().orientation * artouste::vec3{1.0f, 0.0f, 0.0f};
    REQUIRE(avant.x > 0.999f);

    model.reset(pos, 180.0f);
    avant = model.body().orientation * artouste::vec3{1.0f, 0.0f, 0.0f};
    REQUIRE(avant.z > 0.999f);
    REQUIRE(std::fabs(avant.x) < 1e-4f);

    model.reset(pos, 0.0f);
    avant = model.body().orientation * artouste::vec3{1.0f, 0.0f, 0.0f};
    REQUIRE(avant.z < -0.999f);
}

TEST_CASE("Réservoir vidé d'un coup : la turbine s'éteint", "[flight][turbine][carburant]") {
    /* Le réservoir peut tomber à zéro autrement qu'en brûlant : un choc au sol le
       fend (mode zombie, voir CombatMode::applyGroundImpact). La panne sèche doit
       alors couper la turbine comme n'importe quelle extinction. Ce cas passait au
       travers : le test de panne vivait dans la branche de consommation, gardée par
       "carburant > 0", et l'appareil volait indéfiniment à sec. */
    FlightModel model;
    model.reset(500.0f);
    model.turbine().forceRunning();
    Controls commandes{};
    commandes.collective = 0.5f;
    for (int i = 0; i < 240; ++i) {
        model.update(commandes, SIM_DT);
    }
    REQUIRE(model.turbine().turbineFraction() > 0.9f);

    model.drainFuel(2.0f * artouste::physics::FUEL_CAPACITY_L);
    REQUIRE(model.fuelLiters() == 0.0f);

    /* Un pas suffit à déclencher l'extinction... */
    model.update(commandes, SIM_DT);
    CHECK(model.turbine().state() == artouste::physics::Turbine::State::Extinction);

    /* ... et une minute plus tard, turbine et rotor sont arrêtés pour de bon. */
    for (int i = 0; i < 240 * 60; ++i) {
        model.update(commandes, SIM_DT);
    }
    CHECK(model.turbine().turbineFraction() == 0.0f);
    CHECK(model.turbine().rotorFraction() == 0.0f);
    CHECK_FALSE(model.turbine().turning());
}

TEST_CASE("Réservoir vide : la turbine ne redémarre pas", "[flight][turbine][carburant]") {
    /* Sans carburant, appuyer sur la touche de démarrage amorce la séquence, qui
       s'éteint aussitôt : il n'y a plus rien à faire que de quitter le vol. */
    FlightModel model;
    model.reset(500.0f);
    model.drainFuel(2.0f * artouste::physics::FUEL_CAPACITY_L);
    REQUIRE(model.fuelLiters() == 0.0f);

    model.turbine().toggle(); /* le pilote tente un redémarrage */
    const Controls commandes{};
    for (int i = 0; i < 240 * 5; ++i) {
        model.update(commandes, SIM_DT);
    }
    CHECK(model.turbine().turbineFraction() == 0.0f);
    CHECK(model.turbine().rotorFraction() == 0.0f);
}

TEST_CASE("Panne sèche par consommation : la turbine s'éteint aussi",
          "[flight][turbine][carburant]") {
    /* Le chemin d'origine, celui du réservoir vidé par la turbine elle-même, doit
       continuer de fonctionner : on part avec de quoi tenir quelques secondes. */
    FlightModel model;
    model.reset(500.0f);
    model.turbine().forceRunning();
    Controls commandes{};
    commandes.collective = 1.0f; /* pleine puissance : 194 L/h */
    model.drainFuel(artouste::physics::FUEL_CAPACITY_L - 0.2f); /* 0,2 L, ~4 s de vol */

    for (int i = 0; i < 240 * 10; ++i) {
        model.update(commandes, SIM_DT);
    }
    CHECK(model.fuelLiters() == 0.0f);
    CHECK(model.turbine().state() != artouste::physics::Turbine::State::Regime);
}

TEST_CASE("Fond de réservoir : le démarrage est refusé", "[flight][turbine][carburant]") {
    /* La jauge affiche des litres entiers : "0 L" peut cacher un demi-litre, assez
       pour amorcer une séquence de démarrage d'une minute qui s'éteindra juste
       avant le régime de vol, après tout son bruit. On refuse franchement. */
    FlightModel model;
    model.reset(0.0f);
    model.setGroundHeight(0.0f);
    model.drainFuel(artouste::physics::FUEL_CAPACITY_L - 0.3f); /* jauge : 0 L */

    CHECK_FALSE(model.toggleTurbine());
    CHECK(model.turbine().state() == artouste::physics::Turbine::State::Arret);

    /* Et rien ne se met en route au fil des pas suivants. */
    const Controls commandes{};
    for (int i = 0; i < 240 * 5; ++i) {
        model.update(commandes, SIM_DT);
    }
    CHECK(model.turbine().turbineFraction() == 0.0f);
}

TEST_CASE("Réservoir suffisant : le démarrage est accepté", "[flight][turbine][carburant]") {
    FlightModel model;
    model.reset(0.0f);
    model.setGroundHeight(0.0f);
    model.drainFuel(artouste::physics::FUEL_CAPACITY_L - artouste::physics::FUEL_START_MIN_L -
                    1.0f);

    CHECK(model.toggleTurbine());
    CHECK(model.turbine().state() == artouste::physics::Turbine::State::Demarrage);
}

TEST_CASE("Couper la turbine reste toujours possible", "[flight][turbine][carburant]") {
    /* Le garde-fou ne vaut que pour le démarrage : on doit pouvoir couper une
       turbine qui tourne, même avec un fond de réservoir. */
    FlightModel model;
    model.reset(0.0f);
    model.setGroundHeight(0.0f);
    model.turbine().forceRunning();
    model.drainFuel(artouste::physics::FUEL_CAPACITY_L - 0.3f);

    CHECK(model.toggleTurbine());
    CHECK(model.turbine().state() == artouste::physics::Turbine::State::Extinction);
}
