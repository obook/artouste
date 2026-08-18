/*
 * flight_model_regimes_tests.cpp
 * Régimes de vol : flux transversal, tenue en roulis, VNE, vitesse ascensionnelle.
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

TEST_CASE("Le flux transversal donne un roulis gauche pendant la transition",
          "[flight][regimes]") {
    /* Effet transitoire en cloche autour de 14 kt : absent au stationnaire, absent
       une fois la vitesse établie, présent au passage. Sur l'Alouette II (rotor
       horaire vu de dessus), il part à GAUCHE, donc vitesse de roulis négative. La
       documentation américaine annonce l'inverse, ses rotors tournant à l'envers.
       Le manche est plein avant et le cyclique latéral au neutre : tout roulis
       observé vient donc du flux transversal. */
    Controls hover;
    hover.collective = artouste::physics::COLL_HOVER;

    const auto roulisApres = [&](float secondes) {
        FlightModel model;
        model.reset(300.0f);
        model.turbine().forceRunning();
        Controls forward = hover;
        forward.cyclicLongitudinal = 1.0f;
        advance(model, forward, secondes);
        return model.body().angularVelocity.x;
    };

    /* Au stationnaire, rien : sous TRANSVERSE_V_IN le phénomène ne s'amorce pas. */
    FlightModel stationnaire;
    stationnaire.reset(300.0f);
    stationnaire.turbine().forceRunning();
    advance(stationnaire, hover, 2.0f);
    REQUIRE(std::fabs(stationnaire.body().angularVelocity.x) < 0.001f);

    /* Vers 3 s l'appareil traverse la plage (environ 8 m/s) : roulis à gauche. Une
       fois la vitesse installée, il ne doit plus rien en rester. */
    REQUIRE(roulisApres(3.0f) < -0.01f);
    REQUIRE(std::fabs(roulisApres(12.0f)) < 0.01f);
}

TEST_CASE("La vitesse raffermit la tenue en roulis", "[flight][regimes]") {
    /* Même entrée de cyclique latéral, même durée : à vitesse établie le rotor
       travaille dans un air neuf et le stabilisateur mord, donc l'amortissement est
       plus fort et la vitesse de roulis atteinte plus faible qu'au stationnaire.
       C'est la "réponse avion" attendue par la note technique. */
    Controls hover;
    hover.collective = artouste::physics::COLL_HOVER;
    Controls incline = hover;
    incline.cyclicLateral = 1.0f;

    FlightModel stationnaire;
    stationnaire.reset(300.0f);
    stationnaire.turbine().forceRunning();
    advance(stationnaire, hover, 2.0f);
    advance(stationnaire, incline, 0.6f);

    FlightModel croisiere;
    croisiere.reset(300.0f);
    croisiere.turbine().forceRunning();
    Controls forward = hover;
    forward.cyclicLongitudinal = 1.0f;
    advance(croisiere, forward, 12.0f);  /* vitesse établie, ETL plein */
    const float avant = croisiere.body().angularVelocity.x;
    Controls inclineEnVitesse = forward;
    inclineEnVitesse.cyclicLateral = 1.0f;
    advance(croisiere, inclineEnVitesse, 0.6f);
    const float roulisCroisiere = croisiere.body().angularVelocity.x - avant;

    REQUIRE(roulisCroisiere > 0.0f);
    REQUIRE(roulisCroisiere < stationnaire.body().angularVelocity.x);
}

TEST_CASE("Le stationnaire est moins tenu que la croisière", "[flight][regimes]") {
    /* Le rappel artificiel à l'horizontale suit la vitesse air longitudinale : sans
       vitesse, rien ne s'écoule sur le stabilisateur et l'appareil est neutre. À
       plein cyclique latéral, l'inclinaison d'équilibre est donc nettement plus
       forte au stationnaire qu'en mode assisté, où la tenue d'avant est conservée
       telle quelle. Sans cet écart, les deux régimes se pilotaient pareil.

       Mesuré à 8 s, plein cyclique latéral : 14,4 degrés en physique réelle
       contre 12,5 en assisté. Le seuil était à 1,3 tant que TRANSVERSE_ROLL
       valait 900 N.m, car le flux transversal gonflait cet écart d'un tiers ;
       ramené à 450 le 12/08/2026, il ne porte plus la marge, et c'est bien le
       rappel d'assiette que ce test observe désormais. */
    Controls incline;
    incline.collective   = artouste::physics::COLL_HOVER;
    incline.cyclicLateral = 1.0f;

    FlightModel reel;
    reel.reset(300.0f);
    reel.turbine().forceRunning();
    advance(reel, incline, 8.0f);

    FlightModel assiste;
    assiste.reset(300.0f);
    assiste.turbine().forceRunning();
    assiste.setRealFlyPhysicsEnabled(false);
    advance(assiste, incline, 8.0f);

    REQUIRE(tiltAngle(reel) > tiltAngle(assiste) * 1.10f);
}

TEST_CASE("Le décrochage de pale reculante annonce la VNE", "[flight][regimes]") {
    Controls hover;
    hover.collective = artouste::physics::COLL_HOVER;

    /* En vol normal, aucun symptôme. */
    FlightModel model;
    model.reset(600.0f);
    model.turbine().forceRunning();
    advance(model, hover, 2.0f);
    REQUIRE(model.retreatingStall() == 0.0f);

    /* Plein manche avant depuis l'altitude : l'appareil descend en accélérant et
       approche la VNE (195 km/h au niveau de la mer, limite structurelle), les
       symptômes apparaissent. Il faut vraiment pousser : en palier l'appareil
       plafonne sous la VNE, seule la descente permet de s'en approcher. */
    Controls plein = hover;
    plein.cyclicLongitudinal = 1.0f;
    advance(model, plein, 45.0f);
    const float vne = artouste::physics::vneAtAltitudeMs(model.body().position.y);
    const auto& v   = model.body().velocity;
    REQUIRE(std::sqrt(v.x * v.x + v.z * v.z) > artouste::physics::RBS_V_ONSET * vne);
    REQUIRE(model.retreatingStall() > 0.0f);

    /* En mode assisté (physique réelle coupée), aucun symptôme. */
    FlightModel assiste;
    assiste.reset(600.0f);
    assiste.turbine().forceRunning();
    assiste.setRealFlyPhysicsEnabled(false);
    advance(assiste, plein, 25.0f);
    REQUIRE(assiste.retreatingStall() == 0.0f);
}

TEST_CASE("La puissance borne la vitesse ascensionnelle", "[flight][regimes]") {
    /* Monter coûte de la puissance, et la turbine n'en a qu'une quantité finie :
       l'appareil plafonne au taux de montée que laisse l'excédent de puissance, et
       non aux 26 m/s que donnait la seule traînée verticale. La montée s'écrase
       ensuite avec l'altitude, ce qui fait apparaître le plafond sans le coder.

       La cible n'est PAS les 4,2 m/s des fiches : ce chiffre vaut à 1600 kg, alors
       que le modèle vole à 1100 kg. Une fois la puissance au rotor calée sur les
       taux de montée documentés à 1350, 1500 et 1600 kg (voir POWER_ROTOR_W), la
       même équation prédit à 1100 kg environ 9,4 m/s à la vitesse de meilleure
       montée et 4,7 m/s en montée verticale.

       CES DEUX CHIFFRES SONT DIFFÉRENTS, et c'est le coeur du modèle : monter à la
       verticale coûte le plein tarif de puissance induite, alors qu'en avançant le
       rotor brasse de l'air neuf et cette dépense s'effondre. Un taux de montée de
       fiche technique se lit toujours à VY, jamais à la verticale. Confondre les
       deux a déjà coûté un calage : mesurée à la verticale, la montée paraissait
       correcte alors qu'elle était deux fois trop forte en vol de translation. */
    Controls plein;
    plein.collective = 1.0f;

    FlightModel mer;
    mer.reset(0.0f);
    mer.turbine().forceRunning();
    advance(mer, plein, 30.0f);
    const float vsMer = mer.body().velocity.y;  /* montée verticale, sans vitesse */
    REQUIRE(vsMer > 3.5f);
    REQUIRE(vsMer < 6.5f);

    /* En avançant vers VY (environ 72 km/h), la même puissance donne beaucoup plus.
       On tient la vitesse à un simple gain proportionnel sur le manche, faute de
       quoi l'appareil accélère indéfiniment. */
    FlightModel translation;
    translation.reset(0.0f);
    translation.turbine().forceRunning();
    float manche = 0.0f;
    for (int i = 0; i < static_cast<int>(30.0f / SIM_DT); ++i) {
        const auto& v = translation.body().velocity;
        const float vitesse = std::sqrt(v.x * v.x + v.z * v.z);
        manche = artouste::clamp(manche + 0.05f * (20.0f - vitesse) * SIM_DT, -0.6f, 0.6f);
        Controls c;
        c.collective         = 1.0f;
        c.cyclicLongitudinal = manche;
        c.pedals             = artouste::clamp(2.0f * translation.body().angularVelocity.y, -1.0f, 1.0f);
        translation.update(c, SIM_DT);
    }
    REQUIRE(translation.body().velocity.y > vsMer * 1.5f);
    REQUIRE(translation.body().velocity.y < 12.0f);

    /* Le taux de montée se lit à la puissance maximale CONTINUE (tuyère 500 degrés,
       soit une charge de 0,816) et non à plein collectif : c'est dans ces conditions
       que les constructeurs publient leurs chiffres. Le modèle ne représente pas le
       supplément transitoire, la montée à plein levier n'est donc que très
       légèrement supérieure : passer au rouge ne rapporte que de la chaleur, ce qui
       est la bonne leçon à donner au pilote. */
    FlightModel continu;
    continu.reset(0.0f);
    continu.turbine().forceRunning();
    Controls maxiContinu;
    maxiContinu.collective = 0.816f;
    advance(continu, maxiContinu, 30.0f);
    REQUIRE(continu.body().velocity.y > 3.0f);
    REQUIRE(continu.body().velocity.y < vsMer + 0.1f);

    FlightModel altitude;
    altitude.reset(2000.0f);
    altitude.turbine().forceRunning();
    advance(altitude, plein, 30.0f);
    REQUIRE(altitude.body().velocity.y < vsMer * 0.8f);

    /* Le stationnaire, lui, ne paie rien : la pénalité ne porte que sur la montée.
       Sans quoi le collectif de sustentation ne tiendrait plus l'altitude. */
    FlightModel stationnaire;
    stationnaire.reset(50.0f);
    stationnaire.turbine().forceRunning();
    Controls hover;
    const float densite = std::exp(-50.0f / artouste::physics::AIR_DENSITY_SCALE);
    hover.collective = artouste::physics::COLL_HOVER / densite;
    advance(stationnaire, hover, 5.0f);
    REQUIRE(std::fabs(stationnaire.body().position.y - 50.0f) < 1.0f);

    /* Physique réelle coupée (mode assisté, démo) : aucune pénalité, l'appareil
       garde la montée franche qui rend ces modes faciles. */
    FlightModel assiste;
    assiste.reset(0.0f);
    assiste.turbine().forceRunning();
    assiste.setRealFlyPhysicsEnabled(false);
    advance(assiste, plein, 30.0f);
    REQUIRE(assiste.body().velocity.y > 10.0f);
}
