/*
 * weapon_tests.cpp
 * Tests du lance-roquettes (Weapon) : cadence de tir et recharge automatique
 * à munitions infinies. Se teste sans contexte graphique.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/Weapon.hpp"

#include <catch2/catch_test_macros.hpp>

using artouste::app::Weapon;

TEST_CASE("Weapon : cadence et recharge automatique", "[combat][weapon]") {
    Weapon weapon;

    SECTION("gâchette relâchée : aucun tir, munitions intactes") {
        const Weapon::FireResult r = weapon.update(1.0f, false);
        CHECK_FALSE(r.fired);
        CHECK(weapon.ammo() == Weapon::AMMO_MAX);
    }

    SECTION("gâchette tenue : tire et consomme une munition") {
        const Weapon::FireResult r = weapon.update(0.01f, true);
        CHECK(r.fired);
        CHECK(weapon.ammo() == Weapon::AMMO_MAX - 1);
    }

    SECTION("cadence de tir : deux appels rapprochés ne tirent qu'une fois") {
        weapon.update(0.001f, true);
        const int ammoAfterFirst = weapon.ammo();
        const Weapon::FireResult r = weapon.update(0.001f, true); /* trop tôt */
        CHECK_FALSE(r.fired);
        CHECK(weapon.ammo() == ammoAfterFirst);
    }

    SECTION("chargeur vide : recharge automatique puis chargeur plein (munitions infinies)") {
        /* Vide le chargeur à coups de dt longs (une cadence par appel). */
        for (int i = 0; i < Weapon::AMMO_MAX; ++i) {
            weapon.update(1.0f, true);
        }
        CHECK(weapon.ammo() == 0);
        CHECK(weapon.reloading());
        /* Ne tire pas pendant la recharge, même gâchette tenue. */
        const Weapon::FireResult during = weapon.update(0.01f, true);
        CHECK_FALSE(during.fired);
        CHECK(weapon.ammo() == 0);
        /* Pause écoulée : le chargeur repart au plein, munitions de fait infinies. */
        weapon.update(5.0f, false);
        CHECK(weapon.ammo() == Weapon::AMMO_MAX);
    }
}
