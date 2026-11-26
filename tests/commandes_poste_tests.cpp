/*
 * commandes_poste_tests.cpp
 * Tests des poses des commandes du poste 3dexport : manche, collectif et
 * palonniers tournent autour de leur pivot, dans le sens des commandes.
 * Repère racine : x vers la queue, y en haut, z à gauche du pilote.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#include "render/CommandesPoste.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <utility>

using artouste::mat4;
using artouste::vec3;
using artouste::vec4;
using Catch::Approx;
using namespace artouste::render;

namespace {

vec3 appliquer(const mat4& m, const vec3& p) {
    return vec3(m * vec4(p, 1.0f));
}

void verifierIdentite(const mat4& m) {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            CHECK(m[c][r] == Approx(c == r ? 1.0f : 0.0f).margin(1e-6));
        }
    }
}

void verifierFixe(const mat4& m, const vec3& p) {
    const vec3 q = appliquer(m, p);
    CHECK(q.x == Approx(p.x).margin(1e-5));
    CHECK(q.y == Approx(p.y).margin(1e-5));
    CHECK(q.z == Approx(p.z).margin(1e-5));
}

const float SIN15 = std::sin(15.0f * 3.14159265f / 180.0f);
const float SIN10 = std::sin(10.0f * 3.14159265f / 180.0f);

}  /* namespace */

TEST_CASE("Au neutre, les commandes ne bougent pas", "[commandes]") {
    verifierIdentite(poseManche(PIVOT_MANCHE, 0.0f, 0.0f));
    verifierIdentite(poseCollectif(PIVOT_COLLECTIF, 0.0f));
    verifierIdentite(posePalonnier(PIVOT_PALONNIER_DROIT, true, 0.0f));
    verifierIdentite(posePalonnier(PIVOT_PALONNIER_GAUCHE, false, 0.0f));
}

TEST_CASE("Chaque commande tourne autour de son pivot", "[commandes]") {
    verifierFixe(poseManche(PIVOT_MANCHE, 1.0f, -1.0f), PIVOT_MANCHE);
    verifierFixe(poseManche(PIVOT_MANCHE_COPILOTE, 1.0f, -1.0f), PIVOT_MANCHE_COPILOTE);
    verifierFixe(poseCollectif(PIVOT_COLLECTIF, 1.0f), PIVOT_COLLECTIF);
    verifierFixe(poseCollectif(PIVOT_COLLECTIF_COPILOTE, 1.0f), PIVOT_COLLECTIF_COPILOTE);
    verifierFixe(posePalonnier(PIVOT_PALONNIER_DROIT, true, 1.0f), PIVOT_PALONNIER_DROIT);
    verifierFixe(posePalonnier(PIVOT_PALONNIER_GAUCHE, false, -1.0f), PIVOT_PALONNIER_GAUCHE);
    verifierFixe(posePalonnier(PIVOT_PALONNIER_DROIT_COPILOTE, true, 1.0f),
                 PIVOT_PALONNIER_DROIT_COPILOTE);
    verifierFixe(posePalonnier(PIVOT_PALONNIER_GAUCHE_COPILOTE, false, 1.0f),
                 PIVOT_PALONNIER_GAUCHE_COPILOTE);
}

TEST_CASE("Manche en avant : le haut du manche avance", "[commandes]") {
    const vec3 haut = PIVOT_MANCHE + vec3{0.0f, 0.5f, 0.0f};
    const vec3 q    = appliquer(poseManche(PIVOT_MANCHE, 1.0f, 0.0f), haut);
    CHECK(q.x == Approx(haut.x - 0.5f * SIN15).margin(1e-4));
    CHECK(q.z == Approx(haut.z).margin(1e-5));
}

TEST_CASE("Manche à droite : le haut du manche part à droite", "[commandes]") {
    const vec3 haut = PIVOT_MANCHE + vec3{0.0f, 0.5f, 0.0f};
    const vec3 q    = appliquer(poseManche(PIVOT_MANCHE, 0.0f, 1.0f), haut);
    CHECK(q.z == Approx(haut.z - 0.5f * SIN10).margin(1e-4));
    CHECK(q.x == Approx(haut.x).margin(1e-5));
}


TEST_CASE("Collectif levé : la poignée monte, sur les deux sièges", "[commandes]") {
    CHECK(appliquer(poseCollectif(PIVOT_COLLECTIF, 1.0f), POIGNEE_COLLECTIF).y >
          POIGNEE_COLLECTIF.y + 0.03f);
    CHECK(appliquer(poseCollectif(PIVOT_COLLECTIF, 0.5f), POIGNEE_COLLECTIF).y <
          appliquer(poseCollectif(PIVOT_COLLECTIF, 1.0f), POIGNEE_COLLECTIF).y);
    CHECK(appliquer(poseCollectif(PIVOT_COLLECTIF_COPILOTE, 1.0f), POIGNEE_COLLECTIF_COPILOTE).y >
          POIGNEE_COLLECTIF_COPILOTE.y + 0.03f);
    /* Chacun à gauche de son siège : pilote vers z = -0,05, copilote vers +0,59. */
    CHECK(PIVOT_COLLECTIF.z > PIVOT_MANCHE.z);
    CHECK(PIVOT_COLLECTIF_COPILOTE.z > PIVOT_MANCHE_COPILOTE.z);
}

TEST_CASE("Commandes en butée : au-delà, rien ne bouge plus", "[commandes]") {
    const vec3 tete = POIGNEE_COLLECTIF;
    CHECK(appliquer(poseCollectif(PIVOT_COLLECTIF, 2.0f), tete).y ==
          Approx(appliquer(poseCollectif(PIVOT_COLLECTIF, 1.0f), tete).y));
    CHECK(appliquer(poseCollectif(PIVOT_COLLECTIF, -1.0f), tete).y == Approx(tete.y));
    const vec3 haut = PIVOT_MANCHE + vec3{0.0f, 0.5f, 0.0f};
    CHECK(appliquer(poseManche(PIVOT_MANCHE, 3.0f, 0.0f), haut).x ==
          Approx(appliquer(poseManche(PIVOT_MANCHE, 1.0f, 0.0f), haut).x));
}

TEST_CASE("Palonnier à droite : pédale droite en avant, gauche en arrière", "[commandes]") {
    const vec3 droite = PIVOT_PALONNIER_DROIT + vec3{0.0f, 0.15f, 0.0f};
    const vec3 gauche = PIVOT_PALONNIER_GAUCHE + vec3{0.0f, 0.15f, 0.0f};
    const vec3 qd     = appliquer(posePalonnier(PIVOT_PALONNIER_DROIT, true, 1.0f), droite);
    const vec3 qg     = appliquer(posePalonnier(PIVOT_PALONNIER_GAUCHE, false, 1.0f), gauche);
    CHECK(qd.x == Approx(droite.x - 0.15f * SIN15).margin(1e-4));
    CHECK(qg.x == Approx(gauche.x + 0.15f * SIN15).margin(1e-4));
}

TEST_CASE("Manche du copilote : il suit celui du pilote", "[commandes]") {
    const vec3 decalage{0.0f, 0.5f, 0.0f};
    const vec3 pilote   = appliquer(poseManche(PIVOT_MANCHE, 0.7f, -0.4f), PIVOT_MANCHE + decalage);
    const vec3 copilote = appliquer(poseManche(PIVOT_MANCHE_COPILOTE, 0.7f, -0.4f),
                                    PIVOT_MANCHE_COPILOTE + decalage);
    const vec3 ecart    = (copilote - PIVOT_MANCHE_COPILOTE) - (pilote - PIVOT_MANCHE);
    CHECK(glm::length(ecart) == Approx(0.0f).margin(1e-5));
    CHECK(PIVOT_MANCHE_COPILOTE.z > 0.3f);  /* à gauche, devant le siège du copilote */
}

TEST_CASE("Palonniers du copilote : pédale droite en avant à droite", "[commandes]") {
    const vec3 droite = PIVOT_PALONNIER_DROIT_COPILOTE + vec3{0.0f, 0.15f, 0.0f};
    const vec3 gauche = PIVOT_PALONNIER_GAUCHE_COPILOTE + vec3{0.0f, 0.15f, 0.0f};
    CHECK(appliquer(posePalonnier(PIVOT_PALONNIER_DROIT_COPILOTE, true, 1.0f), droite).x <
          droite.x - 0.03f);
    CHECK(appliquer(posePalonnier(PIVOT_PALONNIER_GAUCHE_COPILOTE, false, 1.0f), gauche).x >
          gauche.x + 0.03f);
    /* La pédale droite du copilote est entre ses deux pédales et le pilote. */
    CHECK(PIVOT_PALONNIER_DROIT_COPILOTE.z < PIVOT_PALONNIER_GAUCHE_COPILOTE.z);
}

namespace {

/* Allongement des bras pour atteindre 'prise' : distance épaule-prise sur la
   longueur du bras tendu (au-delà de 1, les os s'étirent). */
float etirement(const OsBras& os, const vec3& prise) {
    const float tendu = glm::length(os.coude - os.epaule) + glm::length(os.main - os.coude);
    return glm::length(prise - os.epaule) / tendu;
}

}  /* namespace */

TEST_CASE("Les mains du pilote atteignent les poignées sans trop s'étirer", "[commandes]") {
    for (float c : {0.0f, 0.5f, 1.0f}) {
        const vec3 prise = appliquer(poseCollectif(PIVOT_COLLECTIF, c), PRISE_COLLECTIF);
        CHECK(etirement(BRAS_PILOTE_GAUCHE, prise) < 1.12f);
    }
    /* Manche : à mi-course au plus (le vol courant), le bras s'étire à peine ;
       en butée, rarement tenue, on tolère un bras nettement allongé. */
    for (float lon : {-0.5f, 0.0f, 0.5f}) {
        for (float lat : {-0.5f, 0.0f, 0.5f}) {
            const vec3 prise = appliquer(poseManche(PIVOT_MANCHE, lon, lat), PRISE_MANCHE);
            CHECK(etirement(BRAS_PILOTE_DROIT, prise) < 1.10f);
        }
    }
    for (float lon : {-1.0f, 1.0f}) {
        for (float lat : {-1.0f, 1.0f}) {
            const vec3 prise = appliquer(poseManche(PIVOT_MANCHE, lon, lat), PRISE_MANCHE);
            CHECK(etirement(BRAS_PILOTE_DROIT, prise) < 1.25f);
        }
    }
}

TEST_CASE("Vue cockpit : le collectif triché monte et la main suit", "[commandes]") {
    const vec3 bas  = appliquer(poseCollectifCockpit(PIVOT_COLLECTIF, POIGNEE_COLLECTIF, 0.0f),
                                POIGNEE_COLLECTIF);
    const vec3 haut = appliquer(poseCollectifCockpit(PIVOT_COLLECTIF, POIGNEE_COLLECTIF, 1.0f),
                                POIGNEE_COLLECTIF);
    /* Au repos, plus long que le vrai levier, dans son axe. */
    CHECK(glm::length(bas - PIVOT_COLLECTIF) ==
          Approx(glm::length(POIGNEE_COLLECTIF - PIVOT_COLLECTIF) * COLLECTIF_COCKPIT_ALLONGE)
              .margin(1e-4));
    /* À plein pas, la poignée monte franchement, et l'oeil du pilote (vue
       cockpit, COCKPIT_EYE_POSTE_FBX en repère racine) la voit plus haut. */
    const vec3 oeil{-3.70f, 0.163f, -0.365f};
    const auto plongee = [&](const vec3& p) {
        const vec3 v = p - oeil;
        return std::atan2(-v.y, std::sqrt(v.x * v.x + v.z * v.z)) * 180.0f / 3.14159265f;
    };
    CHECK(haut.y > bas.y + 0.15f);
    CHECK(plongee(haut) < plongee(bas) - 4.0f);  /* 6 deg avec la course de 15 % */
    verifierFixe(poseCollectifCockpit(PIVOT_COLLECTIF, POIGNEE_COLLECTIF, 0.7f), PIVOT_COLLECTIF);
    /* La main gauche atteint encore la poignée trichée. */
    for (float c : {0.0f, 0.5f, 1.0f}) {
        const vec3 prise =
            appliquer(poseCollectifCockpit(PIVOT_COLLECTIF, POIGNEE_COLLECTIF, c), PRISE_COLLECTIF);
        CHECK(etirement(BRAS_PILOTE_GAUCHE, prise) < 1.15f);
    }
}

TEST_CASE("Les pieds du pilote se posent sur les pédales et les suivent", "[commandes]") {
    for (float pedals : {-1.0f, 0.0f, 1.0f}) {
        const vec3 droit =
            appliquer(posePalonnier(PIVOT_PALONNIER_DROIT, true, pedals), APPUI_PALONNIER_DROIT);
        const vec3 gauche =
            appliquer(posePalonnier(PIVOT_PALONNIER_GAUCHE, false, pedals), APPUI_PALONNIER_GAUCHE);
        /* Le pied reste dans l'axe de sa pédale. */
        CHECK(droit.z == Approx(PIVOT_PALONNIER_DROIT.z).margin(1e-4));
        CHECK(gauche.z == Approx(PIVOT_PALONNIER_GAUCHE.z).margin(1e-4));
        /* Jambes à peine étirées, même pédale poussée. */
        CHECK(etirement(JAMBE_PILOTE_DROITE, droit) < 1.02f);
        CHECK(etirement(JAMBE_PILOTE_GAUCHE, gauche) < 1.02f);
    }
    /* Au neutre, les deux jambes restent pliées (pas tendues d'avance). */
    for (const auto& [jambe, appui] : {std::pair{JAMBE_PILOTE_DROITE, APPUI_PALONNIER_DROIT},
                                       std::pair{JAMBE_PILOTE_GAUCHE, APPUI_PALONNIER_GAUCHE}}) {
        CHECK(etirement(jambe, appui) < TENSION_JAMBE);
    }
}

TEST_CASE("La main gauche saisit la poignée du collectif, elle ne s'y pose pas", "[commandes]") {
    /* Le centre de la main est dans la barre en T (sous son sommet), comme
       la main droite autour de la poignée du manche. */
    CHECK(PRISE_COLLECTIF.y < POIGNEE_COLLECTIF.y);
    CHECK(glm::length(PRISE_COLLECTIF - POIGNEE_COLLECTIF) < 0.03f);
}
