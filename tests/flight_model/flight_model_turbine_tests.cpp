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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>

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
    /* Au régime, la turbine respire autour du nominal (voir BATTEMENT_REGIME) :
       elle ne tient pas 1,0 au chiffre près, une vraie machine non plus. */
    REQUIRE(model.turbine().turbineFraction()
            == Catch::Approx(1.0f).margin(artouste::physics::BATTEMENT_REGIME));
    REQUIRE(model.turbine().rotorFraction() == 0.0f);  /* pales toujours à l'arrêt */

    /* Frein lâché après le délai : le rotor s'accouple puis atteint son régime. */
    advance(model, idle, artouste::physics::ROTOR_BRAKE_DELAY);
    REQUIRE(model.turbine().state() == State::Embrayage);
    advance(model, idle, artouste::physics::ROTOR_ENGAGE_TIME + 1.0f);
    REQUIRE(model.turbine().state() == State::Regime);
    REQUIRE(model.turbine().rotorFraction()
            == Catch::Approx(1.0f).margin(artouste::physics::BATTEMENT_REGIME));

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

    CHECK(model.toggleTurbine() == artouste::physics::ActionTurbine::ReservoirTropBas);
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

    CHECK(model.toggleTurbine() == artouste::physics::ActionTurbine::Faite);
    CHECK(model.turbine().state() == artouste::physics::Turbine::State::Demarrage);
}

TEST_CASE("Couper la turbine demande d'être posé", "[flight][turbine][securite]") {
    using artouste::physics::ActionTurbine;
    using State = artouste::physics::Turbine::State;
    const Controls commandes{};

    /* Prépare un appareil turbine tournante à l'altitude voulue, et laisse un pas
       de simulation établir (ou non) le contact avec le sol. */
    const auto enVolA = [&](float altitude) {
        auto model = std::make_unique<FlightModel>();
        model->reset(altitude);
        model->setGroundHeight(0.0f);
        model->turbine().forceRunning();
        model->update(commandes, SIM_DT);
        return model;
    };

    SECTION("posé, la coupure passe même avec un fond de réservoir") {
        auto model = enVolA(0.0f);
        REQUIRE(model->auSol());
        model->drainFuel(artouste::physics::FUEL_CAPACITY_L - 0.3f);
        CHECK(model->toggleTurbine() == ActionTurbine::Faite);
        CHECK(model->turbine().state() == State::Extinction);
    }

    SECTION("en vol, la coupure est refusée") {
        /* Couper en vol, c'est se mettre en autorotation sans l'avoir voulu :
           une touche pressée par erreur ne doit pas terminer le vol. */
        auto model = enVolA(100.0f);
        REQUIRE_FALSE(model->auSol());
        CHECK(model->toggleTurbine() == ActionTurbine::PasAuSol);
        CHECK(model->turbine().state() == State::Regime);
    }

    SECTION("en vol, la turbine continue de tourner après le refus") {
        auto model = enVolA(100.0f);
        (void)model->toggleTurbine();
        for (int i = 0; i < 240 * 3; ++i) {
            model->update(commandes, SIM_DT);
        }
        CHECK(model->turbine().state() == State::Regime);
        CHECK(model->turbine().rotorFraction() > 0.9f);
    }

    SECTION("rallumer en vol reste permis") {
        /* C'est la procédure après une extinction, pas une fausse manoeuvre. */
        auto model = std::make_unique<FlightModel>();
        model->reset(100.0f);
        model->setGroundHeight(0.0f);
        model->update(commandes, SIM_DT);
        REQUIRE_FALSE(model->auSol());
        REQUIRE(model->turbine().state() == State::Arret);
        CHECK(model->toggleTurbine() == ActionTurbine::Faite);
        CHECK(model->turbine().state() == State::Demarrage);
    }
}

/*
 * Forme des régimes. Une turbine à gaz n'accélère ni ne ralentit linéairement :
 * la montée s'essouffle au lancement, repart à l'allumage, puis arrive au régime
 * de façon asymptotique ; l'extinction chute franchement avant de traîner dans
 * les bas régimes. Ces tests fixent la forme, pas seulement les durées.
 */
TEST_CASE("La montée en régime n'est pas linéaire", "[flight][turbine]") {
    using artouste::physics::Turbine;
    using artouste::physics::ALLUMAGE_INSTANT;

    SECTION("elle part de zéro et arrive pile au régime") {
        CHECK(Turbine::regimeMontee(0.0f) == Catch::Approx(0.0f));
        CHECK(Turbine::regimeMontee(1.0f) == Catch::Approx(1.0f));
        CHECK(Turbine::regimeExtinction(0.0f) == Catch::Approx(1.0f));
        CHECK(Turbine::regimeExtinction(1.0f) == Catch::Approx(0.0f));
    }

    SECTION("elle ne redescend jamais en chemin") {
        float precedent = -1.0f;
        for (int i = 0; i <= 100; ++i) {
            const float v = Turbine::regimeMontee(static_cast<float>(i) / 100.0f);
            CHECK(v > precedent);
            precedent = v;
        }
    }

    SECTION("à mi-parcours, la turbine est déjà bien au-delà de la moitié") {
        /* C'est tout l'écart avec la rampe qu'on remplace : la montée fait le
           gros du chemin tôt, puis s'étire. */
        CHECK(Turbine::regimeMontee(0.5f) > 0.6f);
    }

    SECTION("les derniers pour cent prennent un temps disproportionné") {
        /* Arrivée asymptotique : atteindre 99 % coûte plus des trois quarts du
           temps de démarrage, ce que tout pilote reconnaît à l'oreille. */
        CHECK(Turbine::progresMontee(0.99f) > 0.75f);
    }

    SECTION("l'allumage marque une rupture de pente") {
        /* Avant l'allumage le démarreur s'essouffle, après le carburant relance
           l'accélération : la pente doit remonter en franchissant le seuil. */
        constexpr float h = 0.01f;
        const float avant = Turbine::regimeMontee(ALLUMAGE_INSTANT - h)
                            - Turbine::regimeMontee(ALLUMAGE_INSTANT - 2.0f * h);
        const float apres = Turbine::regimeMontee(ALLUMAGE_INSTANT + 2.0f * h)
                            - Turbine::regimeMontee(ALLUMAGE_INSTANT + h);
        CHECK(apres > avant);
    }

    SECTION("l'extinction chute d'abord, puis traîne") {
        CHECK(Turbine::regimeExtinction(0.5f) < 0.35f);
        CHECK(Turbine::regimeExtinction(0.9f) > 0.0f);
    }

    SECTION("chaque courbe et son inverse se rendent l'aller-retour") {
        for (int i = 0; i <= 20; ++i) {
            const float v = static_cast<float>(i) / 20.0f;
            CHECK(Turbine::regimeMontee(Turbine::progresMontee(v))
                  == Catch::Approx(v).margin(1e-3f));
            CHECK(Turbine::regimeEmbrayage(Turbine::progresEmbrayage(v))
                  == Catch::Approx(v).margin(1e-3f));
            CHECK(Turbine::regimeExtinction(Turbine::progresExtinction(v))
                  == Catch::Approx(v).margin(1e-3f));
        }
    }
}

TEST_CASE("Le régime établi respire au lieu de rester figé", "[flight][turbine]") {
    using artouste::physics::BATTEMENT_REGIME;

    FlightModel    model;
    const Controls idle;
    model.turbine().forceRunning();
    advance(model, idle, 1.0f);

    float mini = 2.0f;
    float maxi = 0.0f;
    for (int i = 0; i < 600; ++i) {
        advance(model, idle, 0.05f);
        const float f = model.turbine().turbineFraction();
        mini = std::min(mini, f);
        maxi = std::max(maxi, f);
        /* Le battement reste dans son amplitude : une turbine qui respire, pas
           une turbine qui pompe. */
        CHECK(std::abs(f - 1.0f) <= BATTEMENT_REGIME + 1e-4f);
        /* Monoarbre : le rotor suit exactement la turbine une fois embrayé. */
        CHECK(model.turbine().rotorFraction() == Catch::Approx(f));
    }
    /* Et il bouge vraiment : sans cela on aurait juste remis une constante. */
    CHECK(maxi - mini > BATTEMENT_REGIME);
}

/*
 * Droop : le régime résulte de l'équilibre entre le couple fourni et celui
 * qu'absorbent les pales. Tirer du collectif charge le rotor, le régime
 * s'affaisse, puis le régulateur le rattrape.
 */
TEST_CASE("Le régime fléchit sous la charge", "[flight][turbine][droop]") {
    using artouste::physics::DROOP_PAS_REF_DEG;
    using artouste::physics::DROOP_STATIQUE;
    using artouste::physics::PAS_MAX_DEG;
    using artouste::physics::Turbine;

    /* Régime moyen sur une seconde : gomme le battement, qui n'a rien à voir
       avec la charge et fausserait une mesure instantanée. */
    const auto regimeMoyen = [](Turbine& t, float pasDeg) {
        float somme = 0.0f;
        for (int i = 0; i < 240; ++i) {
            t.update(SIM_DT, 450.0f, pasDeg);
            somme += t.turbineFraction();
        }
        return somme / 240.0f;
    };
    const auto stabiliser = [&](Turbine& t, float pasDeg) {
        for (int i = 0; i < 240 * 10; ++i) {
            t.update(SIM_DT, 450.0f, pasDeg);
        }
    };

    SECTION("plein pot coûte le statisme annoncé") {
        Turbine t;
        t.forceRunning();
        stabiliser(t, PAS_MAX_DEG);
        CHECK(regimeMoyen(t, PAS_MAX_DEG)
              == Catch::Approx(1.0f - DROOP_STATIQUE).margin(0.002f));
    }

    SECTION("au pas de sustentation, le régulateur tient le nominal") {
        Turbine t;
        t.forceRunning();
        stabiliser(t, DROOP_PAS_REF_DEG);
        CHECK(regimeMoyen(t, DROOP_PAS_REF_DEG) == Catch::Approx(1.0f).margin(0.002f));
    }

    SECTION("sous le pas de sustentation, pas de surrégime") {
        /* Un régime au-dessus du nominal allumerait le voyant à tort. */
        Turbine t;
        t.forceRunning();
        stabiliser(t, 6.0f);
        CHECK(regimeMoyen(t, 6.0f) <= 1.0f + artouste::physics::BATTEMENT_REGIME);
    }

    SECTION("plus la charge est forte, plus le régime est bas") {
        float precedent = 2.0f;
        for (const float pas : {11.0f, 12.0f, 13.0f, 14.0f, 15.0f}) {
            Turbine t;
            t.forceRunning();
            stabiliser(t, pas);
            const float r = regimeMoyen(t, pas);
            CHECK(r < precedent);
            precedent = r;
        }
    }

    SECTION("une action franche creuse le régime, puis il remonte") {
        Turbine t;
        t.forceRunning();
        stabiliser(t, 6.0f);

        /* Collectif tiré d'un coup de la butée basse au plein pot. */
        float creux = 2.0f;
        for (int i = 0; i < 240; ++i) {
            t.update(SIM_DT, 450.0f, PAS_MAX_DEG);
            creux = std::min(creux, t.turbineFraction());
        }
        /* Le creux passe SOUS le statisme : c'est tout l'intérêt du transitoire. */
        CHECK(creux < 1.0f - DROOP_STATIQUE - 0.005f);

        /* Puis le régulateur rattrape et le régime se cale sur le statisme. */
        stabiliser(t, PAS_MAX_DEG);
        CHECK(regimeMoyen(t, PAS_MAX_DEG) > creux + 0.005f);
    }

    SECTION("relâcher le collectif ne creuse rien") {
        /* Décharger la turbine ne peut pas faire plonger le régime. */
        Turbine t;
        t.forceRunning();
        stabiliser(t, PAS_MAX_DEG);
        float mini = 2.0f;
        for (int i = 0; i < 240; ++i) {
            t.update(SIM_DT, 450.0f, 6.0f);
            mini = std::min(mini, t.turbineFraction());
        }
        CHECK(mini >= 1.0f - DROOP_STATIQUE - artouste::physics::BATTEMENT_REGIME);
    }

    SECTION("le rotor suit la turbine au chiffre près (monoarbre)") {
        Turbine t;
        t.forceRunning();
        for (int i = 0; i < 240 * 3; ++i) {
            t.update(SIM_DT, 450.0f, 14.0f);
            CHECK(t.rotorFraction() == Catch::Approx(t.turbineFraction()));
        }
    }
}
