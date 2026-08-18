/*
 * config_completion_tests.cpp
 * Report des options nouvelles dans une configuration ancienne.
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

TEST_CASE("option nouvelle ajoutée à une configuration ancienne", "[config]") {
    const auto dossier = preparerDossier("artouste_cfg_maj", "terrain bordeaux\n");
    const Config cfg = loadConfig(dossier / "config.txt");

    /* Le réglage existant est respecté, et l'option absente garde son défaut. */
    REQUIRE(cfg.terrain == "bordeaux");
    REQUIRE(cfg.checkUpdate);

    const std::string apres = lireTout(dossier / "config.txt");
    /* L'option manquante a été recopiée du modèle, avec sa documentation. */
    REQUIRE(apres.find("verifier_maj 1") != std::string::npos);
    REQUIRE(apres.find("# verifier_maj : recherche d'une mise à jour") != std::string::npos);
    REQUIRE(apres.find("# 0 pour ne rien demander au réseau.") != std::string::npos);
    /* Le choix de l'utilisateur n'a pas été réécrit, et la clé qu'il avait déjà
       n'a pas été ajoutée une seconde fois. */
    REQUIRE(apres.find("terrain bordeaux") != std::string::npos);
    REQUIRE(apres.find("terrain ossau") == std::string::npos);

    std::filesystem::remove_all(dossier);
}

TEST_CASE("configuration complète : le fichier n'est pas touché", "[config]") {
    const auto dossier =
        preparerDossier("artouste_cfg_complet", "terrain bordeaux\nverifier_maj 0\n");
    const std::string avant = lireTout(dossier / "config.txt");

    const Config cfg = loadConfig(dossier / "config.txt");
    REQUIRE_FALSE(cfg.checkUpdate);
    REQUIRE(lireTout(dossier / "config.txt") == avant);

    std::filesystem::remove_all(dossier);
}

TEST_CASE("deux lancements de suite : pas de doublon", "[config]") {
    const auto dossier = preparerDossier("artouste_cfg_deuxfois", "terrain bordeaux\n");
    loadConfig(dossier / "config.txt");
    const std::string apresPremier = lireTout(dossier / "config.txt");
    loadConfig(dossier / "config.txt");
    REQUIRE(lireTout(dossier / "config.txt") == apresPremier);

    std::filesystem::remove_all(dossier);
}

TEST_CASE("configuration en CRLF : les lignes ajoutées le sont aussi", "[config]") {
    /* Fichier enregistré par le Bloc-notes de Windows. Mêler des fins de ligne LF
       aux siennes donnerait un fichier illisible dans certains éditeurs. */
    const auto dossier = preparerDossier("artouste_cfg_crlf", "terrain bordeaux\r\n");
    loadConfig(dossier / "config.txt");

    const std::string apres = lireTout(dossier / "config.txt");
    REQUIRE(apres.find("verifier_maj 1\r\n") != std::string::npos);
    /* Aucun saut de ligne isolé : chaque \n doit être précédé d'un \r. */
    for (std::size_t i = 0; i < apres.size(); ++i) {
        if (apres[i] == '\n') {
            REQUIRE(i > 0);
            REQUIRE(apres[i - 1] == '\r');
        }
    }

    std::filesystem::remove_all(dossier);
}

TEST_CASE("configuration sans saut de ligne final : l'ajout ne colle pas à la dernière clé",
          "[config]") {
    const auto dossier = preparerDossier("artouste_cfg_sansfin", "terrain bordeaux");
    const Config cfg = loadConfig(dossier / "config.txt");
    REQUIRE(cfg.terrain == "bordeaux");

    const std::string apres = lireTout(dossier / "config.txt");
    REQUIRE(apres.find("terrain bordeaux\n") != std::string::npos);

    /* Relu, le fichier complété doit rendre exactement les mêmes réglages. */
    const Config relu = loadConfig(dossier / "config.txt");
    REQUIRE(relu.terrain == "bordeaux");
    REQUIRE(relu.checkUpdate);

    std::filesystem::remove_all(dossier);
}

TEST_CASE("modèle effacé : la configuration est lue quand même", "[config]") {
    /* Sans modèle, il n'y a rien à recopier : les réglages de l'utilisateur
       restent lus, les options nouvelles gardent la valeur par défaut du
       chargeur, et son fichier n'est pas touché. */
    const auto dossier = preparerDossier("artouste_cfg_sansmodele", "terrain bordeaux\n");
    std::filesystem::remove(dossier / "config.default.txt");

    const Config cfg = loadConfig(dossier / "config.txt");
    REQUIRE(cfg.terrain == "bordeaux");
    REQUIRE(cfg.checkUpdate); /* défaut du chargeur, faute de modèle à recopier */
    REQUIRE(lireTout(dossier / "config.txt") == "terrain bordeaux\n");

    std::filesystem::remove_all(dossier);
}
