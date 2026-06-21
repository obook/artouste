/*
 * config_tests.cpp
 * Vérifie le chargeur de configuration "clé valeur" (app::loadConfig) : valeur
 * par défaut quand le fichier est absent, lecture de la clé terrain, et bonne
 * gestion des commentaires et lignes vides.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Config.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>

using artouste::app::Config;
using artouste::app::loadConfig;

namespace {

/* Écrit un fichier de configuration temporaire et renvoie son chemin. */
std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << content;
    return path;
}

}  /* namespace */

TEST_CASE("fichier absent : valeurs par défaut", "[config]") {
    const Config cfg = loadConfig("/chemin/qui/n/existe/pas/config.txt");
    REQUIRE(cfg.terrain == "ossau");
}

TEST_CASE("clé terrain lue", "[config]") {
    const auto   path = writeTemp("artouste_cfg_terrain.txt", "terrain cote-landes\n");
    const Config cfg  = loadConfig(path);
    REQUIRE(cfg.terrain == "cote-landes");
    std::filesystem::remove(path);
}

TEST_CASE("commentaires et lignes vides ignorés", "[config]") {
    const auto path = writeTemp("artouste_cfg_comments.txt",
                                "# un commentaire\n\n   \nterrain ossau\n# autre\n");
    const Config cfg = loadConfig(path);
    REQUIRE(cfg.terrain == "ossau");
    std::filesystem::remove(path);
}

TEST_CASE("valeur vide : la clé garde son défaut", "[config]") {
    const auto   path = writeTemp("artouste_cfg_empty.txt", "terrain   \n");
    const Config cfg  = loadConfig(path);
    REQUIRE(cfg.terrain == "ossau");
    std::filesystem::remove(path);
}
