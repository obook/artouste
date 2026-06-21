/*
 * Config.cpp
 * Lecture du fichier de configuration "clé valeur" (voir Config.hpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Config.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace artouste::app {

namespace {

/* Retire les espaces de début et de fin d'une chaîne (utile car la valeur est le
   reste de la ligne après la clé). */
std::string trim(const std::string& s) {
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

}  /* namespace */

Config loadConfig(const std::filesystem::path& path) {
    Config cfg;
    std::ifstream in(path);
    if (!in) {
        /* Pas de fichier : on garde les valeurs par défaut, sans erreur. */
        return cfg;
    }

    std::string line;
    while (std::getline(in, line)) {
        const std::string clean = trim(line);
        if (clean.empty() || clean[0] == '#') {
            continue;  /* ligne vide ou commentaire */
        }
        std::istringstream iss(clean);
        std::string        key;
        iss >> key;
        std::string value;
        std::getline(iss, value);
        value = trim(value);

        if (key == "terrain") {
            if (!value.empty()) {
                cfg.terrain = value;
            }
        } else if (key == "turbine_demarree") {
            cfg.turbineRunning = (value == "1" || value == "oui" || value == "true");
        } else {
            std::fprintf(stderr, "[Config] clé inconnue ignorée : %s\n", key.c_str());
        }
    }
    return cfg;
}

}  /* namespace artouste::app */
