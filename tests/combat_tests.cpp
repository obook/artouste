/*
 * combat_tests.cpp
 * Tests du mode zombie : intersection rayon/sphère (physics::raySphere),
 * lance-roquettes (Weapon : cadence et recharge), explosion en zone
 * (RocketSystem) et cycle de vie d'un zombie (ZombieHorde). Se teste sans
 * contexte graphique : ni render::Zombies (OpenGL) ni CombatMode (RigidBody
 * complet) ne sont nécessaires pour vérifier cette logique.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/ProjectileSystem.hpp"
#include "app/combat/RocketSystem.hpp"
#include "app/combat/WaveManager.hpp"
#include "app/combat/Weapon.hpp"
#include "app/combat/ZombieHorde.hpp"
#include "physics/Raycast.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using artouste::vec3;
using artouste::app::ProjectileSystem;
using artouste::app::RocketSystem;
using artouste::app::WaveManager;
using artouste::app::Weapon;
using artouste::app::ZombieHorde;
using artouste::physics::raySphere;

TEST_CASE("raySphere : rayon droit vers une sphère", "[combat][raycast]") {
    const vec3 origin{0.0f, 0.0f, 0.0f};
    const vec3 dir{1.0f, 0.0f, 0.0f};

    SECTION("touche une sphère devant") {
        const auto hit = raySphere(origin, dir, vec3{10.0f, 0.0f, 0.0f}, 1.0f);
        REQUIRE(hit.hit);
        CHECK(hit.distance == Catch::Approx(9.0f));
    }

    SECTION("rate une sphère décalée hors du rayon") {
        const auto hit = raySphere(origin, dir, vec3{10.0f, 5.0f, 0.0f}, 1.0f);
        CHECK_FALSE(hit.hit);
    }

    SECTION("rate une sphère derrière l'origine") {
        const auto hit = raySphere(origin, dir, vec3{-10.0f, 0.0f, 0.0f}, 1.0f);
        CHECK_FALSE(hit.hit);
    }

    SECTION("touche une sphère tangente au bord du rayon") {
        const auto hit = raySphere(origin, dir, vec3{10.0f, 1.0f, 0.0f}, 1.0f);
        REQUIRE(hit.hit);
        CHECK(hit.distance == Catch::Approx(10.0f).margin(0.01f));
    }

    SECTION("origine à l'intérieur de la sphère : distance nulle ou positive") {
        const auto hit = raySphere(origin, dir, vec3{0.0f, 0.0f, 0.0f}, 5.0f);
        REQUIRE(hit.hit);
        CHECK(hit.distance >= 0.0f);
    }
}

namespace {
/* Terrain plat à l'altitude 0, pour isoler la logique de tir/dégâts du
   recalage de relief (déjà couvert ailleurs, voir ZombieHorde::update). */
float flatGround(float, float) noexcept {
    return 0.0f;
}
}  /* namespace */

TEST_CASE("ZombieHorde : dégâts, mort et despawn", "[combat][zombie]") {
    ZombieHorde horde;
    horde.spawn(vec3{0.0f, 0.0f, 0.0f});
    REQUIRE(horde.count() == 1);

    SECTION("un coup sous-létal blesse sans tuer") {
        horde.applyDamage(0, 10.0f);
        CHECK(horde.zombies()[0].health == Catch::Approx(90.0f));
        CHECK(horde.zombies()[0].state == ZombieHorde::State::Alive);
        CHECK(horde.buildInstanceMatrices().size() == 1);
    }

    SECTION("un coup létal passe en Dying puis despawn après l'anim de chute") {
        horde.applyDamage(0, 1000.0f);
        CHECK(horde.zombies()[0].state == ZombieHorde::State::Dying);
        CHECK(horde.buildInstanceMatrices().size() == 1);  /* encore dessiné en train de tomber */

        /* Joueur loin et hors de portée : sans incidence sur un zombie Dying
           (il ne marche ni ne lance plus, voir update). */
        const vec3 farPlayer{5000.0f, 100.0f, 5000.0f};

        /* Avant la fin de l'anim de chute : toujours présent. */
        horde.update(0.5f, farPlayer, 100.0f, 1.0f, flatGround);
        CHECK(horde.count() == 1);

        /* Après (durée totale > 1 s, voir DEATH_ANIM_DURATION_S) : despawn. */
        horde.update(0.6f, farPlayer, 100.0f, 1.0f, flatGround);
        CHECK(horde.count() == 0);
        CHECK(horde.buildInstanceMatrices().empty());
    }

    SECTION("un zombie déjà mort n'encaisse plus de dégâts") {
        horde.applyDamage(0, 1000.0f);  /* Dying */
        horde.applyDamage(0, 1000.0f);  /* ne doit rien changer d'autre */
        CHECK(horde.zombies()[0].state == ZombieHorde::State::Dying);
    }

    SECTION("indice hors bornes : sans effet") {
        horde.applyDamage(42, 10.0f);
        CHECK(horde.zombies()[0].health == Catch::Approx(100.0f));
    }
}

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
        const int                ammoAfterFirst = weapon.ammo();
        const Weapon::FireResult r              = weapon.update(0.001f, true);  /* trop tôt */
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

TEST_CASE("RocketSystem : explosion au sol et dégâts de zone", "[combat][rocket]") {
    /* Sol plat au niveau de la mer : la roquette explose en franchissant y = 0. */
    const auto flat = [](float, float) { return 0.0f; };

    SECTION("une roquette tirée vers le sol tue les zombies dans le rayon") {
        ZombieHorde horde;
        horde.spawn(vec3{0.0f, 0.0f, 0.0f});                                    /* au point d'impact */
        horde.spawn(vec3{RocketSystem::EXPLOSION_RADIUS_M - 0.5f, 0.0f, 0.0f}); /* dans le rayon */
        horde.spawn(vec3{RocketSystem::EXPLOSION_RADIUS_M + 5.0f, 0.0f, 0.0f}); /* hors rayon */

        /* Tir droit vers le bas depuis 20 m au-dessus du zombie central : point
           d'impact déterministe (x = z = 0), indépendant de la vitesse. */
        RocketSystem rockets;
        const vec3   origin{0.0f, 20.0f, 0.0f};
        const vec3   dir{0.0f, -1.0f, 0.0f};
        rockets.spawn(origin, dir);

        /* Avance jusqu'à ce qu'une explosion se produise (borne de sécurité). */
        int explosions = 0;
        for (int i = 0; i < 2000 && explosions == 0; ++i) {
            explosions += rockets.update(0.01f, flat, horde).explosions;
        }
        CHECK(explosions == 1);

        /* Les deux premiers zombies (dans le rayon) sont touchés (Alive ->
           Dying), le troisième (hors rayon) reste intact. */
        CHECK(horde.zombies()[0].state != ZombieHorde::State::Alive);
        CHECK(horde.zombies()[1].state != ZombieHorde::State::Alive);
        CHECK(horde.zombies()[2].state == ZombieHorde::State::Alive);
    }
}

TEST_CASE("ZombieHorde : marche vers le joueur et jets de boulettes toxiques",
         "[combat][zombie][ai]") {
    ZombieHorde horde;
    horde.spawn(vec3{100.0f, 0.0f, 0.0f});  /* loin sur l'axe X */
    const vec3 player{0.0f, 0.0f, 0.0f};

    SECTION("un zombie vivant avance vers le joueur") {
        const float distAvant = horde.zombies()[0].position.x;
        horde.update(1.0f, player, 100.0f /* AGL au-dessus du plafond : pas de jet */, 1.0f,
                    flatGround);
        CHECK(horde.zombies()[0].position.x < distAvant);
        CHECK(horde.zombies()[0].position.x > 0.0f);  /* pas de dépassement du joueur */
    }

    SECTION("le facteur de vitesse accélère la marche") {
        ZombieHorde lent;
        lent.spawn(vec3{100.0f, 0.0f, 0.0f});
        ZombieHorde rapide;
        rapide.spawn(vec3{100.0f, 0.0f, 0.0f});

        lent.update(1.0f, player, 100.0f, 1.0f, flatGround);
        rapide.update(1.0f, player, 100.0f, 2.0f, flatGround);
        CHECK(rapide.zombies()[0].position.x < lent.zombies()[0].position.x);
    }

    SECTION("hors de portée : marche mais ne lance pas") {
        horde.zombies()[0].position.x = 200.0f;  /* > TOXIC_RANGE_MAX_M (60 m) */
        const auto requests = horde.update(0.1f, player, 0.0f /* sous le plafond */, 1.0f,
                                          flatGround);
        CHECK(requests.empty());
    }

    SECTION("au-dessus du plafond d'altitude : à portée mais ne lance pas") {
        const auto requests = horde.update(0.1f, vec3{95.0f, 0.0f, 0.0f}, 100.0f /* > 35 m */,
                                          1.0f, flatGround);
        CHECK(requests.empty());
    }

    SECTION("à portée, sous le plafond, hors cooldown : lance") {
        const auto requests = horde.update(0.1f, vec3{95.0f, 0.0f, 0.0f}, 0.0f, 1.0f, flatGround);
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].target.x == Catch::Approx(95.0f));
        /* Le cooldown est réarmé : un appel immédiat suivant ne relance pas. */
        const auto requests2 =
            horde.update(0.01f, vec3{95.0f, 0.0f, 0.0f}, 0.0f, 1.0f, flatGround);
        CHECK(requests2.empty());
    }
}

TEST_CASE("ProjectileSystem : trajectoire, impact et expiration", "[combat][projectile]") {
    ProjectileSystem projectiles;
    const vec3        heliCenter{0.0f, 5.0f, 0.0f};
    constexpr float    heliRadius = 2.5f;

    SECTION("aucun projectile : aucun dégât") {
        const float damage = projectiles.update(0.1f, heliCenter, heliRadius);
        CHECK(damage == 0.0f);
        CHECK(projectiles.count() == 0);
    }

    SECTION("trajectoire vers la cible : touche l'appareil resté sur place") {
        projectiles.spawn(vec3{-30.0f, 0.0f, 0.0f}, heliCenter);
        REQUIRE(projectiles.count() == 1);

        float totalDamage = 0.0f;
        bool  hit          = false;
        for (int i = 0; i < 600 && !hit; ++i) {  /* jusqu'à 5 s à 120 Hz, largement assez */
            const float damage = projectiles.update(1.0f / 120.0f, heliCenter, heliRadius);
            totalDamage += damage;
            if (damage > 0.0f) {
                hit = true;
            }
        }
        CHECK(hit);
        CHECK(totalDamage > 0.0f);
        CHECK(projectiles.count() == 0);  /* retiré après impact */
    }

    SECTION("appareil loin de la trajectoire : pas d'impact, expire au bout d'un moment") {
        projectiles.spawn(vec3{-30.0f, 0.0f, 0.0f}, vec3{-30.0f, 0.0f, 500.0f});
        REQUIRE(projectiles.count() == 1);

        float totalDamage = 0.0f;
        for (int i = 0; i < 600; ++i) {  /* 5 s : dépasse MAX_LIFETIME_S (4 s) */
            totalDamage += projectiles.update(1.0f / 120.0f, heliCenter, heliRadius);
        }
        CHECK(totalDamage == 0.0f);
        CHECK(projectiles.count() == 0);  /* despawn de sécurité (durée de vie) */
    }

    SECTION("buildInstances reflète le nombre de projectiles actifs") {
        projectiles.spawn(vec3{0.0f, 0.0f, 0.0f}, vec3{10.0f, 0.0f, 0.0f});
        projectiles.spawn(vec3{0.0f, 0.0f, 0.0f}, vec3{-10.0f, 0.0f, 0.0f});
        CHECK(projectiles.buildInstances().size() == 2);
    }
}

namespace {
/* Écrit un zombies.txt temporaire dans un dossier dédié (WaveManager attend
   un dossier de carte, comme CombatMode) et renvoie ce dossier. */
std::filesystem::path writeTempSpawnDir(const std::string& dirName, const std::string& content) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / dirName;
    std::filesystem::create_directories(dir);
    std::ofstream out(dir / "zombies.txt");
    out << content;
    return dir;
}
}  /* namespace */

TEST_CASE("WaveManager : première vague, escalade et score", "[combat][waves]") {
    const auto dir = writeTempSpawnDir("artouste_waves_test", "10 0\n20 0\n30 0\n");

    SECTION("fichier absent : start() renvoie faux, horde inchangée") {
        WaveManager waves;
        ZombieHorde horde;
        CHECK_FALSE(waves.start("/chemin/qui/n/existe/pas", horde));
        CHECK(horde.count() == 0);
    }

    SECTION("première vague peuplée d'un coup, pas de spawn échelonné") {
        WaveManager waves;
        ZombieHorde horde;
        REQUIRE(waves.start(dir, horde));
        CHECK(waves.waveNumber() == 1);
        CHECK(horde.count() == 5);  /* BASE_ZOMBIES */
        CHECK(waves.score() == 0);  /* vague 1 pas encore survécue */
    }

    SECTION("vague suivante déclenchée par l'extermination, difficulté accrue") {
        WaveManager waves;
        ZombieHorde horde;
        REQUIRE(waves.start(dir, horde));

        horde.clear();  /* simule l'extermination de la vague 1 */
        const float difficulty = waves.update(0.1f, horde);
        CHECK(waves.waveNumber() == 2);
        CHECK(waves.score() == 1);      /* vague 1 survécue */
        CHECK(difficulty > 1.0f);       /* difficulté croissante dès la vague 2 */
        CHECK(horde.count() == 0);      /* transition détectée, spawn pas encore joué */

        /* Draine le spawn échelonné de la vague 2 (BASE + ZOMBIES_STEP = 8). */
        for (int i = 0; i < 20; ++i) {
            waves.update(0.6f, horde);
        }
        CHECK(horde.count() == 8);
    }

    SECTION("délai maximal dépassé : vague suivante même sans horde vide") {
        WaveManager waves;
        ZombieHorde horde;
        REQUIRE(waves.start(dir, horde));
        waves.update(95.0f, horde);  /* > WAVE_MAX_DURATION_S (90 s), anti-blocage */
        CHECK(waves.waveNumber() == 2);
    }

    std::filesystem::remove_all(dir);
}
