/*
 * mise_a_jour_tests.cpp
 * Tests de l'analyse faite par la recherche de mise à jour (app/MiseAJour.cpp) :
 * lecture du tag dans la réponse de GitHub, puis comparaison de deux numéros de
 * version. La requête réseau elle-même n'est pas testée ici (libcurl n'est pas
 * liée aux tests) : seule la partie qui décide compte, et elle ne dépend de rien.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "app/MiseAJour.hpp"

#include <catch2/catch_test_macros.hpp>

using artouste::app::extraireTagName;
using artouste::app::versionPlusRecente;

TEST_CASE("Le tag de la release est extrait de la reponse JSON", "[maj]") {
    const std::string reponse =
        R"({"url":"https://api.github.com/repos/obook/artouste/releases/1",)"
        R"("tag_name":"v0.29.0","name":"Artouste 0.29.0","draft":false})";
    REQUIRE(extraireTagName(reponse) == "v0.29.0");
}

TEST_CASE("Une reponse sans tag exploitable ne donne rien", "[maj]") {
    REQUIRE(extraireTagName("").empty());
    REQUIRE(extraireTagName(R"({"message":"Not Found"})").empty());
    /* Champ present mais nul (depot sans release publiee). */
    REQUIRE(extraireTagName(R"({"tag_name":null})").empty());
    /* Chaine de valeur jamais fermee : reponse tronquee. */
    REQUIRE(extraireTagName(R"({"tag_name":"v0.29.0)").empty());
}

TEST_CASE("Une version publiee plus haute est vue comme plus recente", "[maj]") {
    REQUIRE(versionPlusRecente("v0.29.0", "0.28.0"));
    REQUIRE(versionPlusRecente("0.28.1", "0.28.0"));
    REQUIRE(versionPlusRecente("1.0.0", "0.99.99"));
    /* Le compteur de commits de la version affichee au menu est ignore. */
    REQUIRE(versionPlusRecente("v0.29.0", "0.28.0.483"));
}

TEST_CASE("Une version identique ou plus ancienne ne declenche rien", "[maj]") {
    REQUIRE_FALSE(versionPlusRecente("v0.28.0", "0.28.0"));
    REQUIRE_FALSE(versionPlusRecente("v0.28.0", "0.28.0.483"));
    REQUIRE_FALSE(versionPlusRecente("v0.27.9", "0.28.0"));
    /* Build local en avance sur la derniere release publiee : rien a proposer. */
    REQUIRE_FALSE(versionPlusRecente("v0.28.0", "0.29.0"));
}

TEST_CASE("Une version illisible ne derange pas le pilote", "[maj]") {
    REQUIRE_FALSE(versionPlusRecente("", "0.28.0"));
    REQUIRE_FALSE(versionPlusRecente("latest", "0.28.0"));
    REQUIRE_FALSE(versionPlusRecente("v", "0.28.0"));
    REQUIRE_FALSE(versionPlusRecente("v0.29.0", "version de developpement"));
    REQUIRE_FALSE(versionPlusRecente("v0.29.", "0.28.0"));
}

TEST_CASE("Une version a deux champs completes par un zero", "[maj]") {
    REQUIRE(versionPlusRecente("v1.0", "0.28.0"));
    REQUIRE_FALSE(versionPlusRecente("v0.28", "0.28.0"));
    REQUIRE(versionPlusRecente("v0.28.1", "0.28"));
}
