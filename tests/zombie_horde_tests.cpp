/*
 * zombie_horde_tests.cpp
 * Tests du cycle de vie d'un zombie (ZombieHorde) : dégâts, mort et despawn,
 * marche vers le joueur, jets de pneus toxiques, et cas du largueur
 * (boss). Se teste sans contexte graphique (ni render::Zombies ni CombatMode
 * ne sont nécessaires).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/ZombieHorde.hpp"
#include "app/combat/ZombieHordeReglages.hpp"

#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using artouste::vec3;
using artouste::app::ZombieHorde;

namespace {
/* Terrain plat à l'altitude 0, pour isoler la logique de tir/dégâts du
   recalage de relief (déjà couvert ailleurs, voir ZombieHorde::update). */
float flatGround(float, float) noexcept {
    return 0.0f;
}
} /* namespace */

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
        CHECK(horde.buildInstanceMatrices().size() == 1); /* encore dessiné en train de tomber */

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
        horde.applyDamage(0, 1000.0f); /* Dying */
        horde.applyDamage(0, 1000.0f); /* ne doit rien changer d'autre */
        CHECK(horde.zombies()[0].state == ZombieHorde::State::Dying);
    }

    SECTION("indice hors bornes : sans effet") {
        horde.applyDamage(42, 10.0f);
        CHECK(horde.zombies()[0].health == Catch::Approx(100.0f));
    }
}

TEST_CASE("ZombieHorde : marche vers le joueur et jets de pneus toxiques",
          "[combat][zombie][ai]") {
    ZombieHorde horde;
    horde.spawn(vec3{100.0f, 0.0f, 0.0f}); /* loin sur l'axe X */
    const vec3 player{0.0f, 0.0f, 0.0f};

    SECTION("un zombie vivant avance vers le joueur") {
        const float distAvant = horde.zombies()[0].position.x;
        horde.update(
            1.0f, player, 100.0f /* AGL au-dessus du plafond : pas de jet */, 1.0f, flatGround);
        CHECK(horde.zombies()[0].position.x < distAvant);
        CHECK(horde.zombies()[0].position.x > 0.0f); /* pas de dépassement du joueur */
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
        horde.zombies()[0].position.x = 200.0f; /* > TOXIC_RANGE_MAX_M (60 m) */
        const auto requests =
            horde.update(0.1f, player, 0.0f /* sous le plafond */, 1.0f, flatGround);
        CHECK(requests.empty());
    }

    SECTION("au-dessus du plafond d'altitude : à portée mais ne lance pas") {
        const auto requests =
            horde.update(0.1f, vec3{95.0f, 0.0f, 0.0f}, 100.0f /* > 35 m */, 1.0f, flatGround);
        CHECK(requests.empty());
    }

    SECTION("à portée, sous le plafond, hors cooldown : lance") {
        const auto requests = horde.update(0.1f, vec3{95.0f, 0.0f, 0.0f}, 0.0f, 1.0f, flatGround);
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].target.x == Catch::Approx(95.0f));
        /* Le cooldown est réarmé : un appel immédiat suivant ne relance pas. */
        const auto requests2 = horde.update(0.01f, vec3{95.0f, 0.0f, 0.0f}, 0.0f, 1.0f, flatGround);
        CHECK(requests2.empty());
    }
}

TEST_CASE("ZombieHorde : largueur (boss)", "[combat][zombie][boss]") {
    ZombieHorde horde;

    SECTION("un largueur est repérable, très résistant, et avance à la mesure de ses jambes") {
        horde.spawnBrood(vec3{100.0f, 0.0f, 0.0f});
        REQUIRE(horde.broodAlive());
        CHECK(horde.zombies()[0].type == ZombieHorde::Type::Brood);
        CHECK(horde.zombies()[0].health == Catch::Approx(ZombieHorde::BROOD_HEALTH));
        CHECK(horde.broodHealthPct() == Catch::Approx(1.0f));
        CHECK(horde.zombies()[0].scale == Catch::Approx(ZombieHorde::BROOD_SCALE));

        /* Chaque zombie avance à la vitesse de SA propre animation (mesurée,
           voir VARIANTES_MARCHE) mise à l'échelle de sa taille. Le largueur
           n'échappe pas à la règle : c'est le même modèle agrandi, donc sa
           foulée est BROOD_SCALE fois plus longue et il doit couvrir
           BROOD_SCALE fois plus de terrain. Toute autre valeur le fait patiner. */
        const vec3 player{0.0f, 0.0f, 0.0f};
        horde.update(1.0f, player, 100.0f, 1.0f, flatGround);

        const float avance  = 100.0f - horde.zombies()[0].position.x;
        const float attendu = horde.zombies()[0].clipSpeedMs * ZombieHorde::BROOD_SCALE;
        CHECK(avance == Catch::Approx(attendu));

        /* Un marcheur du même modèle, lui, avance d'une foulée simple. */
        ZombieHorde marcheurs;
        marcheurs.spawn(vec3{100.0f, 0.0f, 0.0f});
        marcheurs.update(1.0f, player, 100.0f, 1.0f, flatGround);
        const float avanceMarcheur = 100.0f - marcheurs.zombies()[0].position.x;
        CHECK(avanceMarcheur == Catch::Approx(marcheurs.zombies()[0].clipSpeedMs));
    }

    SECTION("les neuf personnages sortent du chapeau, et aucun ne reste planté") {
        /* On garde les neuf silhouettes du pack, y compris les cinq qui n'ont
           pas de marche propre : elles empruntent l'allure plancher (voir
           VITESSE_PLANCHER_MS). Sans ce plancher, deux variantes sur neuf
           resteraient immobiles et ne rejoindraient jamais le joueur. */
        ZombieHorde  beaucoup;
        std::set<unsigned int> vues;
        for (int i = 0; i < 400; ++i) {
            beaucoup.spawn(vec3{static_cast<float>(i), 0.0f, 0.0f});
        }
        for (const ZombieHorde::Zombie& z : beaucoup.zombies()) {
            const unsigned int variante = z.kind % artouste::app::VARIANTES_PACK;
            vues.insert(variante);
            CHECK(z.clipSpeedMs == Catch::Approx(artouste::app::vitesseVariante(variante)));
            CHECK(z.clipSpeedMs >= artouste::app::SEUIL_VRAIE_MARCHE_MS);
        }
        CHECK(vues.size() == artouste::app::VARIANTES_PACK);
    }

    SECTION("une vraie marche garde SA vitesse, seules les autres empruntent") {
        /* Le remplacement ne doit toucher que les animations qui n'avancent
           pas : dégrader une marche mesurée juste la ferait glisser pour rien. */
        using artouste::app::SEUIL_VRAIE_MARCHE_MS;
        using artouste::app::VITESSE_EMPRUNTEE_MS;
        using artouste::app::VITESSE_VARIANTE_MS;
        using artouste::app::vitesseVariante;
        for (unsigned int v = 0; v < artouste::app::VARIANTES_PACK; ++v) {
            if (VITESSE_VARIANTE_MS[v] >= SEUIL_VRAIE_MARCHE_MS) {
                CHECK(vitesseVariante(v) == Catch::Approx(VITESSE_VARIANTE_MS[v]));
            } else {
                CHECK(vitesseVariante(v) == Catch::Approx(VITESSE_EMPRUNTEE_MS));
            }
        }
    }

    SECTION("une roquette ne suffit pas : il en faut cinq") {
        horde.spawnBrood(vec3{0.0f, 0.0f, 0.0f});
        for (int i = 0; i < 4; ++i) {
            horde.applyDamage(0, 1000.0f); /* RocketSystem::BLAST_DAMAGE */
            CHECK(horde.broodAlive());
        }
        horde.applyDamage(0, 1000.0f);
        CHECK_FALSE(horde.broodAlive());
        CHECK(horde.broodHealthPct() == Catch::Approx(0.0f));
    }

    SECTION("une teinte d'yeux par zombie, rouge pour le largueur") {
        horde.spawn(vec3{0.0f, 0.0f, 0.0f});
        horde.spawnBrood(vec3{50.0f, 0.0f, 0.0f});
        const auto tints = horde.buildEyeTints();
        /* Même ordre et même filtrage que les matrices et les "kind" : le rendu
           indexe les trois tableaux ensemble pour poser les lueurs. */
        REQUIRE(tints.size() == horde.buildInstanceMatrices().size());
        REQUIRE(tints.size() == 2);
        /* Marcheur : dominante verte. Largueur : rouge, et une lueur plus
           large, à l'échelle de sa silhouette. */
        CHECK(tints[0].color.g > tints[0].color.r);
        CHECK(tints[1].color.r > tints[1].color.g);
        CHECK(tints[1].radius > tints[0].radius);
    }

    SECTION("les marcheurs lâchés par le largueur ont aussi les yeux rouges") {
        horde.spawn(vec3{0.0f, 0.0f, 0.0f});           /* venu du bord : vert */
        horde.spawnBroodling(vec3{10.0f, 0.0f, 0.0f}); /* lâché : rouge */
        const auto tints = horde.buildEyeTints();
        REQUIRE(tints.size() == 2);
        CHECK(tints[0].color.g > tints[0].color.r);
        CHECK(tints[1].color.r > tints[1].color.g);
        /* Rouge comme le largueur, mais la lueur garde la taille d'un marcheur. */
        CHECK(tints[1].radius == Catch::Approx(tints[0].radius));
    }

    SECTION("le largueur abattu emporte ce qu'il a lâché, et lui seul") {
        horde.spawnBrood(vec3{0.0f, 0.0f, 0.0f});
        horde.spawnBroodling(vec3{3.0f, 0.0f, 0.0f});
        horde.spawnBroodling(vec3{6.0f, 0.0f, 0.0f});
        horde.spawn(vec3{60.0f, 0.0f, 0.0f}); /* venu du bord : épargné */

        const auto eclates = horde.killBroodlings();
        REQUIRE(eclates.size() == 2);
        CHECK(eclates[0].x == Catch::Approx(3.0f));
        CHECK(eclates[1].x == Catch::Approx(6.0f));
        CHECK(horde.zombies()[1].state == ZombieHorde::State::Dying);
        CHECK(horde.zombies()[2].state == ZombieHorde::State::Dying);
        CHECK(horde.zombies()[3].state == ZombieHorde::State::Alive);

        /* Rien à tuer deux fois : un second appel ne renvoie plus personne. */
        CHECK(horde.killBroodlings().empty());
    }

    SECTION("le regard s'éteint avec la chute") {
        horde.spawn(vec3{0.0f, 0.0f, 0.0f});
        horde.applyDamage(0, 100.0f);  /* passe en Dying : la lueur décroît */
        REQUIRE(horde.zombies()[0].state == ZombieHorde::State::Dying);
        const float debut = horde.buildEyeTints()[0].color.g;
        horde.update(0.5f, vec3{1000.0f, 0.0f, 0.0f}, 100.0f, 1.0f, flatGround);
        const auto tints = horde.buildEyeTints();
        REQUIRE(tints.size() == 1);
        CHECK(tints[0].color.g < debut);
    }
}
