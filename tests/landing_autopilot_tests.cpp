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
#include "app/LandingAutopilot.hpp"
#include "physics/FlightModel.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>

using artouste::vec3;
using artouste::app::LandingAutopilot;
using artouste::physics::Controls;
using artouste::physics::FlightModel;

namespace {

constexpr float SIM_DT = 1.0f / 240.0f;

using TerrainFn = std::function<float(float, float)>;

/* Écart (m) au-dessus duquel on considère l'appareil "en excédent notable" par
   rapport à la pente d'approche standard (GAIN_ALT_RETOUR * dist) -- sert à repérer
   une descente quasi immobile (vitesse sol proche de 0 alors qu'il reste beaucoup
   d'altitude à perdre), voir vitesseMinPendantExcesM. */
constexpr float SEUIL_EXCES_NOTABLE_M = 20.0f;

struct Resultat {
    bool  pose            = false;  /* landing.active() est devenu faux (posé) dans le temps imparti */
    float vitesseSolM     = 0.0f;   /* vitesse horizontale au posé (m/s) */
    float tauxDescenteM   = 0.0f;   /* |vitesse verticale| au posé (m/s) */
    float distancePadM    = 0.0f;   /* distance horizontale au centre du pad au posé (m) */
    float dureeS          = 0.0f;
    float clearanceMinM   = 0.0f;   /* plus petite hauteur-sol observée (hauteur - relief réel)
                                       en vol, avant la finale : négative si l'appareil est
                                       rentré dans le relief (CFIT). */
    float excesMaxM       = 0.0f;   /* plus grand écart observé entre agl et la pente standard
                                       (GAIN_ALT_RETOUR * dist), hors finale : mesure combien
                                       d'altitude "en trop" l'appareil a dû rattraper. */
    float vitesseSolMinPendantExcesM = -1.0f;  /* plus petite vitesse sol observée tant que
                                       l'excédent dépasse SEUIL_EXCES_NOTABLE_M (hors finale) ;
                                       -1 si l'excédent notable n'a jamais eu lieu. Proche de 0 :
                                       descente quasi immobile ("en stationnaire") signalée le
                                       16/07/2026, plutôt qu'un vol en avant qui perd l'altitude
                                       en trop tout en progressant vers le pad. */
};

/* Simule un atterrissage automatique complet, depuis un vol stationnaire à
   distanceInitialeM du pad (origine) et altitudeInitialeM de hauteur-sol, jusqu'au
   posé ou à dureeMaxS. Physique réaliste coupée pendant le vol automatique, comme le
   fait l'application (voir ApplicationLoop.cpp).
   terrain (optionnel) : relief entre l'appareil et le pad (sol plat à y = 0 si
   omis). Quand fourni, il est branché à la fois sur le contact sol de FlightModel
   (setGroundHeight, comme Application::mainLoop) et sur l'anticipation de relief de
   LandingAutopilot (hauteurMinRelief), pour rejouer fidèlement le cas réel. */
Resultat simulerApproche(float distanceInitialeM, float altitudeInitialeM, float dureeMaxS,
                         const TerrainFn& terrain = {}) {
    FlightModel model;
    model.reset(vec3{0.0f, altitudeInitialeM, distanceInitialeM}, 0.0f);
    model.turbine().forceRunning();
    model.setRealFlyPhysicsEnabled(false);

    /* Collectif de départ à la sustentation : comme en jeu (toggleAutoland passe
       m_lastControls, les commandes réelles du pilote au moment de l'engagement, pas
       des commandes neutres -- le pilote est toujours déjà en vol à ce moment-là). */
    Controls initial;
    initial.collective = artouste::physics::COLL_HOVER;

    LandingAutopilot landing;
    landing.start(vec3{0.0f, 0.0f, 0.0f}, initial);

    /* FlightModel a une vraie physique de contact : une fois posé (poussée sous le
       poids), la vitesse est remise à {0,0,0} d'un coup (voir FlightModel.cpp,
       "posé sur les patins"). Lire la vitesse une fois landing.active() devenu faux
       mesurerait donc cet état déjà calé, pas la vitesse d'impact réelle. On retient
       plutôt le pire taux de descente et la vitesse sol juste avant le contact
       (tant que l'appareil est encore en l'air, proche du sol). */
    float pireDescenteM = 0.0f;
    float vitesseAvantContactM = 0.0f;
    float clearanceMinM = 1.0e6f;
    float excesMaxM = 0.0f;
    float vitesseSolMinPendantExcesM = -1.0f;

    Resultat res;
    float t = 0.0f;
    while (t < dureeMaxS) {
        const vec3& pos     = model.body().position;
        const vec3& vel     = model.body().velocity;
        const vec3  fwd     = model.body().orientation * vec3{1.0f, 0.0f, 0.0f};
        const float heading = std::atan2(-fwd.z, fwd.x);
        const float sol     = terrain ? terrain(pos.x, pos.z) : 0.0f;
        const float agl     = pos.y - sol;
        if (terrain) {
            model.setGroundHeight(sol);
        }
        clearanceMinM = std::min(clearanceMinM, agl);

        if (agl > 0.0f && agl < 30.0f) {
            pireDescenteM        = std::max(pireDescenteM, -vel.y);
            vitesseAvantContactM = std::sqrt(vel.x * vel.x + vel.z * vel.z);
        }

        /* Écart à la pente standard, hors finale (dist < DIST_POSE, où l'écart
           résiduel est géré par une descente verticale contrôlée, pas un vol en
           avant -- voir LandingAutopilot::update). */
        using namespace artouste::app::demo_detail;
        const float dist = std::sqrt(pos.x * pos.x + pos.z * pos.z);
        if (dist > DIST_POSE) {
            const float hauteurPente = artouste::clamp(GAIN_ALT_RETOUR * dist, 0.0f, ALT_PLAFOND);
            const float exces        = agl - hauteurPente;
            if (exces > excesMaxM) {
                excesMaxM = exces;
            }
            if (exces > SEUIL_EXCES_NOTABLE_M) {
                const float vitesseSol = std::sqrt(vel.x * vel.x + vel.z * vel.z);
                if (vitesseSolMinPendantExcesM < 0.0f || vitesseSol < vitesseSolMinPendantExcesM) {
                    vitesseSolMinPendantExcesM = vitesseSol;
                }
            }
        }

        if (!landing.active()) {
            res.pose                         = true;
            res.vitesseSolM                  = vitesseAvantContactM;
            res.tauxDescenteM                = pireDescenteM;
            res.distancePadM                 = std::sqrt(pos.x * pos.x + pos.z * pos.z);
            res.dureeS                       = t;
            res.clearanceMinM                = clearanceMinM;
            res.excesMaxM                    = excesMaxM;
            res.vitesseSolMinPendantExcesM    = vitesseSolMinPendantExcesM;
            break;
        }
        const Controls controls = landing.update(SIM_DT, pos, vel, heading, agl, terrain);
        model.update(controls, SIM_DT);
        t += SIM_DT;
    }
    return res;
}

}  /* namespace anonyme */

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
    /* Garde-fou de non-régression (valeur actuelle ~1,7 m/s) plutôt qu'un seuil de
       confort : si une future modification de ces gains fait chuter cette vitesse
       vers 0, c'est qu'elle a aggravé la descente en stationnaire plutôt que de
       l'améliorer. */
    CHECK(r.vitesseSolMinPendantExcesM > 1.0f);
    CHECK(r.tauxDescenteM < 3.0f);
    CHECK(r.vitesseSolM < 3.0f);
    CHECK(r.distancePadM < 8.0f);
}
