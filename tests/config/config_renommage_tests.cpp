/*
 * config_renommage_tests.cpp
 * Renommage des anciennes clés et cohérence des trois listes.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "config/aide_config.hpp"

#include "app/Config.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using artouste::app::Config;
using artouste::app::loadConfig;
using essais_config::clesDuModele;
using essais_config::effacerTemp;
using essais_config::lireTout;
using essais_config::preparerDossier;
using essais_config::writeTemp;

TEST_CASE("option renommée : le réglage de l'utilisateur est conservé", "[config]") {
    /* Le jour où "arbres" deviendrait "vegetation", le fichier d'un ancien
       utilisateur doit suivre : sans cela sa clé serait rejetée comme inconnue et
       ses arbres coupés reviendraient, puisque le nouveau nom vaut 1 par défaut. */
    const std::map<std::string, std::string> renommages = {{"arbres", "vegetation"}};
    const auto path = writeTemp("artouste_cfg_renom",
                                "# mes reglages\n"
                                "terrain bordeaux\n"
                                "arbres 0\n"
                                "# fin\n");

    REQUIRE(artouste::app::renommerAnciennesCles(path, renommages) == 1);

    const std::string apres = lireTout(path);
    /* La valeur, la place et tout le reste du fichier sont intacts. */
    REQUIRE(apres == "# mes reglages\nterrain bordeaux\nvegetation 0\n# fin\n");

    effacerTemp(path);
}

TEST_CASE("option renommée : ni la mise en page ni les fins de ligne ne bougent", "[config]") {
    const std::map<std::string, std::string> renommages = {{"arbres", "vegetation"}};
    /* Indentation, espaces multiples et CRLF du Bloc-notes : rien ne doit changer
       hors le nom de la clé. */
    const auto path = writeTemp("artouste_cfg_renom_forme", "  arbres    0\r\nterrain ossau\r\n");

    REQUIRE(artouste::app::renommerAnciennesCles(path, renommages) == 1);
    REQUIRE(lireTout(path) == "  vegetation    0\r\nterrain ossau\r\n");

    effacerTemp(path);
}

TEST_CASE("option renommée déjà présente sous son nom actuel : pas de doublon", "[config]") {
    const std::map<std::string, std::string> renommages = {{"arbres", "vegetation"}};
    const auto path = writeTemp("artouste_cfg_renom_double", "arbres 0\nvegetation 1\n");

    REQUIRE(artouste::app::renommerAnciennesCles(path, renommages) == 1);

    const std::string apres = lireTout(path);
    /* L'ancienne ligne est neutralisée et expliquée, la ligne actuelle est intacte,
       et le fichier ne porte qu'une seule clé "vegetation" active. */
    REQUIRE(apres.find("\n# arbres 0") != std::string::npos);
    REQUIRE(apres.find("vegetation 1\n") != std::string::npos);
    REQUIRE(apres.find("Ancien nom") != std::string::npos);

    effacerTemp(path);
}

TEST_CASE("aucun renommage à faire : le fichier n'est pas réécrit", "[config]") {
    const std::map<std::string, std::string> renommages = {{"arbres", "vegetation"}};
    const auto path = writeTemp("artouste_cfg_renom_rien", "terrain bordeaux\n");
    const std::string avant = lireTout(path);

    REQUIRE(artouste::app::renommerAnciennesCles(path, renommages) == 0);
    REQUIRE(lireTout(path) == avant);
    /* Aucun fichier intermédiaire ne doit traîner. */
    REQUIRE_FALSE(std::filesystem::exists(path.string() + ".nouveau"));

    effacerTemp(path);
}

TEST_CASE("table de renommage cohérente avec les clés du chargeur", "[config]") {
    for (const auto& [ancien, actuel] : artouste::app::clesRenommees()) {
        INFO("renommage " << ancien << " -> " << actuel);
        /* Le nom d'arrivée doit exister... */
        REQUIRE(artouste::app::clesConnues().count(actuel) == 1);
        /* ... et l'ancien avoir vraiment disparu, sans quoi les deux noms
           vivraient en même temps et le renommage serait une perte de réglage. */
        REQUIRE(artouste::app::clesConnues().count(ancien) == 0);
        REQUIRE(clesDuModele().count(ancien) == 0);
    }
}

TEST_CASE("toute option du modèle est connue du chargeur", "[config]") {
    /* Sens modèle -> code. Une option décrite par le modèle mais que le chargeur
       ignore serait recopiée chez l'utilisateur, puis rejetée au lancement suivant
       avec un "clé inconnue" qui ressemble à un bug. */
    for (const std::string& cle : clesDuModele()) {
        INFO("clé du modèle absente de app::clesConnues() : " << cle);
        REQUIRE(artouste::app::clesConnues().count(cle) == 1);
    }
}

TEST_CASE("toute option du chargeur est documentée dans le modèle", "[config]") {
    /* Sens code -> modèle. Une option que le code sait lire mais que le modèle
       ne mentionne pas ne serait proposée à personne : ni aux nouveaux venus (qui
       reçoivent une copie du modèle), ni aux anciens (dont le fichier est complété
       depuis ce même modèle). */
    const std::set<std::string> modele = clesDuModele();
    for (const std::string& cle : artouste::app::clesConnues()) {
        INFO("clé du code absente de assets/config.default.txt : " << cle);
        REQUIRE(modele.count(cle) == 1);
    }
}
