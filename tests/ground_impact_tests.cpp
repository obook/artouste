/*
 * ground_impact_tests.cpp
 * Contact de l'appareil avec le sol : mesure de la vitesse d'arrivée par le
 * modèle de vol (physics::FlightModel) et carburant qu'en fait fuir le mode zombie
 * (app::CombatMode). Se teste sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/BonusSphereReglages.hpp"
#include "app/combat/CombatMode.hpp"
#include "physics/constants.hpp"
#include "physics/RigidBody.hpp"
#include "physics/FlightModel.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using artouste::vec3;
using artouste::app::CombatMode;
namespace physics = artouste::physics;

using artouste::physics::Controls;
using artouste::physics::FlightModel;

namespace {

/* Sol plat au niveau de la mer, comme dans les autres tests de combat. */
float solPlat(float, float) {
    return 0.0f;
}

/* Dossier de carte minimal (un point d'apparition) : sans lui, le mode zombie
   ne démarre pas et le contact au sol ne coûterait rien. */
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

TEST_CASE("CombatMode : carburant perdu au contact du sol", "[combat][contact]") {
    const auto dir = dossierCarteTemporaire("artouste_contact_test");
    CombatMode combat;
    combat.start(dir, solPlat);
    REQUIRE(combat.active());
    REQUIRE(combat.healthPct() == Catch::Approx(1.0f));

    SECTION("un posé normal ne coûte rien") {
        CHECK(combat.applyGroundImpact(2.0f, physics::FUEL_CAPACITY_L) == Catch::Approx(0.0f));
        CHECK_FALSE(combat.soundEvents().impacted);
    }

    SECTION("le choc fend le réservoir, il n'entame pas la vie") {
        /* La vie ne se perd que face aux zombies. Un posé brutal se paie en
           kérosène, donc en minutes de vol restantes. */
        const float litres = combat.applyGroundImpact(10.0f, physics::FUEL_CAPACITY_L);
        CHECK(litres > 0.0f);
        CHECK(combat.healthPct() == Catch::Approx(1.0f));
        CHECK_FALSE(combat.gameOver());
        CHECK(combat.soundEvents().impacted);
    }

    SECTION("un contact trop doux pour se voir ne s'entend pas non plus") {
        /* La jauge affiche des litres entiers : sous un demi-litre, le joueur
           entendrait le choc sans rien voir bouger, ce qui se lit comme un bug.
           Ces vitesses dépassent pourtant le seuil du posé. */
        for (const float vitesse : {3.1f, 3.3f}) {
            CombatMode doux;
            doux.start(dir, solPlat);
            INFO("arrivée à " << vitesse << " m/s");
            CHECK(doux.applyGroundImpact(vitesse, physics::FUEL_CAPACITY_L) == Catch::Approx(0.0f));
            CHECK_FALSE(doux.soundEvents().impacted);
        }
    }

    SECTION("dès que la fuite se voit, elle s'entend") {
        /* Réciproque : toute fuite d'au moins un demi-litre doit faire du bruit,
           sans quoi le carburant baisserait sans que le joueur sache pourquoi. */
        for (const float vitesse : {3.6f, 6.0f, 12.0f}) {
            CombatMode dur;
            dur.start(dir, solPlat);
            INFO("arrivée à " << vitesse << " m/s");
            CHECK(dur.applyGroundImpact(vitesse, physics::FUEL_CAPACITY_L) >= 0.5f);
            CHECK(dur.soundEvents().impacted);
        }
    }

    SECTION("plus le contact est rapide, plus il coûte") {
        const float leger = combat.applyGroundImpact(6.0f, physics::FUEL_CAPACITY_L);
        CombatMode autre;
        autre.start(dir, solPlat);
        CHECK(autre.applyGroundImpact(12.0f, physics::FUEL_CAPACITY_L) > leger);
    }

    SECTION("un vrai crash vide presque le réservoir, mais laisse la réserve") {
        /* Sanction du crash, à la place de l'ancienne mort instantanée : il ne
           reste que le fond du réservoir. Mais JAMAIS zéro -- sans ce plafond,
           la courbe au carré dépasse la contenance dès 20 m/s et clouait
           l'appareil au sol, la partie perdue d'un seul contact. */
        const float perdu = combat.applyGroundImpact(20.0f, physics::FUEL_CAPACITY_L);
        CHECK(perdu == Catch::Approx(physics::FUEL_CAPACITY_L - CombatMode::IMPACT_RESERVE_L));
        CHECK(physics::FUEL_CAPACITY_L - perdu == Catch::Approx(CombatMode::IMPACT_RESERVE_L));
    }

    SECTION("le choc le plus violent ne prend pas plus que le crash ordinaire") {
        CombatMode enorme;
        enorme.start(dir, solPlat);
        CHECK(enorme.applyGroundImpact(80.0f, physics::FUEL_CAPACITY_L)
              == Catch::Approx(physics::FUEL_CAPACITY_L - CombatMode::IMPACT_RESERVE_L));
    }

    SECTION("un réservoir déjà sous la réserve ne perd plus rien") {
        /* Rien à prendre : le choc s'entend quand même (il est violent), mais
           il ne peut pas creuser sous ce qui reste. */
        CombatMode presqueVide;
        presqueVide.start(dir, solPlat);
        const float restant = CombatMode::IMPACT_RESERVE_L - 5.0f;
        CHECK(presqueVide.applyGroundImpact(20.0f, restant) == Catch::Approx(0.0f));
        CHECK(presqueVide.soundEvents().impacted);
    }

    SECTION("entre les deux, le choc s'arrête pile sur la réserve") {
        CombatMode moitie;
        moitie.start(dir, solPlat);
        const float restant = CombatMode::IMPACT_RESERVE_L + 40.0f;
        CHECK(moitie.applyGroundImpact(20.0f, restant) == Catch::Approx(40.0f));
    }

    SECTION("tirer ne peut jamais couper la turbine") {
        /* Les sphères bleues naissent des zombies abattus : un joueur à sec qui
           ne peut plus tirer ne peut plus se ravitailler. Le canon devient donc
           gratuit sur les derniers litres. */
        CombatMode           canon;
        canon.start(dir, solPlat);
        physics::RigidBody   appareil;
        appareil.position = vec3{0.0f, 50.0f, 0.0f};
        canon.update(1.0f / 60.0f, appareil, true, solPlat);
        REQUIRE(canon.soundEvents().fired);

        const float reserve = artouste::app::TIR_RESERVE_L;
        CHECK(canon.shotFuelBurn(physics::FUEL_CAPACITY_L)
              == Catch::Approx(artouste::app::SHOT_FUEL_L));
        CHECK(canon.shotFuelBurn(reserve + 1.0f) == Catch::Approx(1.0f));
        CHECK(canon.shotFuelBurn(reserve) == Catch::Approx(0.0f));
        CHECK(canon.shotFuelBurn(1.0f) == Catch::Approx(0.0f));
    }

    SECTION("un coup qui ne part pas ne coûte rien") {
        CombatMode silencieux;
        silencieux.start(dir, solPlat);
        CHECK(silencieux.shotFuelBurn(physics::FUEL_CAPACITY_L) == Catch::Approx(0.0f));
    }

    SECTION("hors combat, le sol ne coûte rien") {
        CombatMode inactif;
        CHECK(inactif.applyGroundImpact(30.0f, physics::FUEL_CAPACITY_L) == Catch::Approx(0.0f));
        CHECK_FALSE(inactif.soundEvents().impacted);
    }
}
