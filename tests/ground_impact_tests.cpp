/*
 * ground_impact_tests.cpp
 * Contact de l'appareil avec le sol : mesure de la vitesse d'arrivée par le
 * modèle de vol (physics::FlightModel) et dégâts qu'en tire le mode zombie
 * (app::CombatMode). Se teste sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/CombatMode.hpp"
#include "physics/FlightModel.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using artouste::app::CombatMode;
using artouste::physics::Controls;
using artouste::physics::FlightModel;

namespace {

/* Sol plat au niveau de la mer, comme dans les autres tests de combat. */
float solPlat(float, float) {
    return 0.0f;
}

/* Dossier de carte minimal (un point d'apparition) : sans lui, le mode zombie
   ne démarre pas et les dégâts n'auraient pas de vie à entamer. */
std::filesystem::path dossierCarteTemporaire(const std::string& nom) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / nom;
    std::filesystem::create_directories(dir);
    std::ofstream out(dir / "zombies.txt");
    out << "40 0\n";
    return dir;
}

/* Laisse tomber l'appareil depuis 'altitude' jusqu'au sol, turbine coupée, et
   renvoie la vitesse d'arrivée relevée. */
float chuteLibre(FlightModel& vol, float altitude) {
    vol.reset(altitude);
    vol.setGroundHeight(0.0f);
    const Controls neutre{};
    for (int i = 0; i < 240 * 5; ++i) {
        vol.update(neutre, 1.0f / 240.0f);
    }
    return vol.consumeGroundImpact();
}

} /* namespace */

TEST_CASE("FlightModel : vitesse d'arrivée au contact du sol", "[physics][contact]") {
    FlightModel vol;

    SECTION("une chute libre laisse une vitesse d'arrivée, lue une seule fois") {
        const float vitesse = chuteLibre(vol, 20.0f);
        /* Chute libre de 20 m : environ 20 m/s en théorie, moins avec la
           traînée. On vérifie l'ordre de grandeur, pas la valeur exacte. */
        CHECK(vitesse > 10.0f);
        CHECK(vitesse < 25.0f);
        /* Consommée : la relire ne la redonne pas. */
        CHECK(vol.consumeGroundImpact() == Catch::Approx(0.0f));
    }

    SECTION("rester posé ne produit aucun nouveau contact") {
        chuteLibre(vol, 5.0f);
        const Controls neutre{};
        for (int i = 0; i < 240; ++i) {
            vol.update(neutre, 1.0f / 240.0f);
        }
        CHECK(vol.consumeGroundImpact() == Catch::Approx(0.0f));
    }

    SECTION("repositionner l'appareil oublie le contact non lu") {
        chuteLibre(vol, 20.0f);  /* valeur laissée en attente */
        vol.reset();
        CHECK(vol.consumeGroundImpact() == Catch::Approx(0.0f));
    }
}

TEST_CASE("CombatMode : dégâts d'un contact avec le sol", "[combat][contact]") {
    const auto  dir = dossierCarteTemporaire("artouste_contact_test");
    CombatMode  combat;
    combat.start(dir, solPlat);
    REQUIRE(combat.active());
    REQUIRE(combat.healthPct() == Catch::Approx(1.0f));

    SECTION("un posé normal ne coûte rien") {
        combat.applyGroundImpact(2.0f);
        CHECK(combat.healthPct() == Catch::Approx(1.0f));
        CHECK_FALSE(combat.soundEvents().impacted);
    }

    SECTION("un contact trop doux pour se voir ne s'entend pas non plus") {
        /* Le HUD affiche la vie en pourcentage entier : sous un demi-point, le
           joueur entend le choc et ne voit rien bouger, ce qui se lit comme un
           bug (il cherche alors la perte ailleurs, dans le carburant). Ces
           vitesses dépassent pourtant le seuil du posé. */
        for (const float vitesse : {3.2f, 3.5f, 4.0f}) {
            CombatMode doux;
            doux.start(dir, solPlat);
            doux.applyGroundImpact(vitesse);
            INFO("arrivée à " << vitesse << " m/s");
            CHECK(doux.healthPct() == Catch::Approx(1.0f));
            CHECK_FALSE(doux.soundEvents().impacted);
        }
    }

    SECTION("dès que le choc se voit, il s'entend") {
        /* Réciproque : tout contact qui coûte au moins un demi-point doit faire du
           bruit. Sans quoi le joueur perdrait de la vie sans savoir pourquoi. */
        for (const float vitesse : {4.5f, 6.0f, 12.0f}) {
            CombatMode dur;
            dur.start(dir, solPlat);
            dur.applyGroundImpact(vitesse);
            INFO("arrivée à " << vitesse << " m/s");
            CHECK(dur.healthPct() < 1.0f);
            CHECK(dur.soundEvents().impacted);
        }
    }

    SECTION("un choc entame la vie et fait du bruit") {
        combat.applyGroundImpact(10.0f);
        CHECK(combat.healthPct() < 1.0f);
        CHECK(combat.healthPct() > 0.0f);
        CHECK(combat.soundEvents().impacted);
    }

    SECTION("plus le contact est rapide, plus il coûte") {
        combat.applyGroundImpact(6.0f);
        const float apresLeger = combat.healthPct();

        CombatMode autre;
        autre.start(dir, solPlat);
        autre.applyGroundImpact(12.0f);
        CHECK(autre.healthPct() < apresLeger);
    }

    SECTION("un contact violent termine la partie") {
        combat.applyGroundImpact(30.0f);
        CHECK(combat.healthPct() == Catch::Approx(0.0f));
        CHECK(combat.gameOver());
    }

    SECTION("partie perdue : plus rien à encaisser") {
        combat.applyGroundImpact(30.0f);
        REQUIRE(combat.gameOver());
        combat.applyGroundImpact(30.0f); /* ne doit pas replonger dans les dégâts */
        CHECK(combat.healthPct() == Catch::Approx(0.0f));
    }
}
