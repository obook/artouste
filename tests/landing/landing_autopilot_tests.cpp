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

#include "app/DemoPilotDetail.hpp"
#include "landing/banc_approche.hpp"

#include <catch2/catch_test_macros.hpp>

using essais_pose::Resultat;
using essais_pose::simulerApproche;

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

TEST_CASE("Atterrissage automatique : col avec un relief plus haut que la pente directe", "[landing]") {
    /* Cas Col du Tourmalet (bug signalé le 16/07/2026) : le pad est au fond d'un col,
       la ligne directe vers le pad peut couper un flanc de montagne plus haut que le
       pad avant de l'atteindre. Sans anticipation, l'appareil ne réagit qu'à la
       hauteur-sol sous lui à l'instant présent (voir hauteurMinRelief dans
       DemoPilotDetail.hpp) : le relief surgit trop tard pour que le collectif ait le
       temps de faire grimper l'appareil au-dessus, d'où un CFIT.
       Relief : une arête (gaussienne) en travers de la route directe, à mi-chemin, à
       60 m de haut -- largement au-dessus de la pente à 6 % (0,06 * 250 = 15 m à cette
       distance) que suivrait un pilote sans anticipation du relief. */
    const auto col = [](float /*x*/, float z) noexcept -> float {
        constexpr float ARETE_Z  = 400.0f;
        constexpr float SIGMA    = 60.0f;
        constexpr float HAUTEUR  = 60.0f;
        const float     dz       = z - ARETE_Z;
        return HAUTEUR * std::exp(-(dz * dz) / (2.0f * SIGMA * SIGMA));
    };
    const Resultat r = simulerApproche(600.0f, 36.0f, 180.0f, col);
    REQUIRE(r.pose);
    /* Jamais rentré dans le relief (tolérance numérique pour l'intégration à pas
       fixe) : preuve directe qu'il n'y a pas eu de CFIT sur l'arête. */
    CHECK(r.clearanceMinM > -0.5f);
    /* Franchir le relief impose de rattraper plus d'altitude en descente qu'une
       approche normale (voir les deux tests précédents, seuil 2,5 m/s) : on reste
       néanmoins sous le seuil réel d'atterrissage dur pour un train à patins (~2,4 à
       3 m/s, voir le premier test) -- une arrivée un peu plus ferme, jamais un choc. */
    CHECK(r.tauxDescenteM < 3.0f);
    CHECK(r.vitesseSolM < 3.0f);
    CHECK(r.distancePadM < 8.0f);   // sous le rayon réel du pad (PAD_PLATFORM_RADIUS_M, Hapi.hpp)
}

TEST_CASE("Atterrissage automatique : relief loin du pad, limite connue (descente lente mais sûre)", "[landing]") {
    /* Bug signalé le 16/07/2026 (en jeu, après le correctif du CFIT ci-dessus) :
       "parfois l'approche auto est très haute, donc descente en stationnaire sur
       100, 200 ou 300 mètres". hauteurMinRelief visait directement l'altitude du
       relief le plus haut sondé, quelle que soit sa distance -- un sommet très en
       amont (loin du pad, encore loin de l'appareil) faisait donc grimper
       l'appareil immédiatement à cette hauteur, bien avant d'en avoir besoin, avec
       tout cet excédent à perdre ensuite en descente lente plafonnée par le GPWS
       (collectifApprocheGpws), pendant que le filet de vitesse horizontale
       (vitesseMinApproche) s'éteint avant la fin.
       PENTE_MONTEE_RELIEF (DemoPilotDetail.hpp) atténue la montée prématurée, mais
       ne réduit pas l'excédent à perdre une fois le relief franchi -- purement
       géométrique (hauteur du relief + marge, moins la pente standard à cette
       distance du pad), indépendant de la façon dont on y est monté. Une tentative
       d'élargir la fenêtre à plein régime de vitesseMinApproche (au-delà de
       DIST_APPROCHE_FINALE) a été essayée et a empiré la mesure ci-dessous : elle
       repousse simplement la coupure de vitesse plus près du pad, où elle devient
       plus brutale. Non résolu : nécessite de repenser plus profondément
       l'écoulement de l'excédent (ouvert, voir [[atterrissage-automatique-v0210]]).
       Reste sûr en l'état (pas de CFIT, taux de descente sous le seuil réel
       d'atterrissage dur) : c'est une limite de confort, pas de sécurité.
       Relief : un sommet de 150 m à mi-chemin d'une approche de 900 m (le rayon
       maximal de recherche de pad, PAD_SEARCH_RADIUS_M) -- largement au-dessus de
       la pente à 6 % (0,06 * 450 = 27 m à cette distance). */
    const auto sommet = [](float /*x*/, float z) noexcept -> float {
        constexpr float SOMMET_Z = 450.0f;
        constexpr float SIGMA    = 80.0f;
        constexpr float HAUTEUR  = 150.0f;
        const float     dz       = z - SOMMET_Z;
        return HAUTEUR * std::exp(-(dz * dz) / (2.0f * SIGMA * SIGMA));
    };
    const Resultat r = simulerApproche(900.0f, 54.0f, 240.0f, sommet);
    REQUIRE(r.pose);
    CHECK(r.clearanceMinM > -0.5f);  // toujours pas de CFIT : la sécurité est acquise
    /* Garde-fou de non-régression plutôt qu'un seuil de confort : si une future
       modification de ces gains fait chuter cette vitesse vers 0, c'est qu'elle a
       aggravé la descente en stationnaire plutôt que de l'améliorer.
       Valeur mesurée le 09/08/2026 après le passage au bilan de puissance : 1,03 m/s
       (1,7 auparavant). La cellule ayant retrouvé sa traînée physique, elle freine
       moins toute seule, et il a fallu amorcer la décélération plus tôt
       (GAIN_V_DIST de 0,14 à 0,11) pour la ramener au-dessus de 1 m/s. Le seuil est
       posé à 0,5 et non à 1,0 : cette mesure est un minimum le long d'une
       trajectoire, donc très sensible aux gains (0,47 m/s à GAIN_V_DIST = 0,14,
       0,71 à 0,09), et un garde-fou collé à la valeur du jour se déclencherait pour
       du bruit. Ce qu'il doit attraper, c'est un effondrement vers zéro. */
    CHECK(r.vitesseSolMinPendantExcesM > 0.5f);
    CHECK(r.tauxDescenteM < 3.0f);
    CHECK(r.vitesseSolM < 3.0f);
    CHECK(r.distancePadM < 8.0f);
}

TEST_CASE("Atterrissage automatique : pad perché sur un plateau", "[landing][pad-perche]") {
    /* Cas du Tourmalet : le pad est posé sur un plateau nettement plus haut que le
       terrain d'approche. Le plateau n'existe dans terrainHeight que dans les 8 m
       de PAD_PLATFORM_RADIUS_M, et hauteurMinRelief le rate (sonde tous les 25 m).
       Une pente d'approche référée au sol local menait donc l'appareil sous le
       niveau du plateau, avant que le contact ne le remette dessus d'un coup. */
    constexpr float ALT_PAD = 60.0f;
    const auto plateau = [](float x, float z) noexcept -> float {
        constexpr float RAYON = 8.0f;
        return (x * x + z * z <= RAYON * RAYON) ? ALT_PAD : 0.0f;
    };
    /* Départ à 900 m et 120 m de hauteur-sol, soit 60 m au-dessus du plateau. */
    const Resultat r = simulerApproche(900.0f, 120.0f, 240.0f, plateau, ALT_PAD);
    REQUIRE(r.pose);
    /* Le point à vérifier : l'appareil ne descend jamais sous le niveau du pad. */
    CHECK(r.altMinSurPadM > -0.5f);
    CHECK(r.tauxDescenteM < 3.0f);
    CHECK(r.vitesseSolM < 3.0f);
    CHECK(r.distancePadM < 8.0f);
}
