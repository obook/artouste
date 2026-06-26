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
#include <filesystem>
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
        /* Pas de config personnelle : on la crée en recopiant le modèle versionné
           config.default.txt (rangé à côté), comme un "config.dist" recopié au premier
           lancement. L'utilisateur édite ensuite sa copie, qui n'est pas suivie par git.
           Si le modèle manque aussi, on garde les valeurs par défaut, sans erreur. */
        const std::filesystem::path modele = path.parent_path() / "config.default.txt";
        std::error_code             ec;
        if (std::filesystem::exists(modele, ec)) {
            std::filesystem::copy_file(modele, path, ec);
            if (!ec) {
                std::printf("[Config] %s absent : créé depuis %s\n",
                            path.filename().string().c_str(), modele.filename().string().c_str());
                in.clear();
                in.open(path);
            } else {
                std::fprintf(stderr, "[Config] création de %s impossible : %s\n",
                             path.filename().string().c_str(), ec.message().c_str());
            }
        }
        if (!in) {
            return cfg;
        }
    }

    std::string line;
    bool        firstLine = true;
    while (std::getline(in, line)) {
        if (firstLine) {
            /* Un éditeur Windows (Bloc-notes) peut enregistrer en UTF-8 avec BOM ;
               on retire ces 3 octets de tête pour ne pas fausser la première ligne. */
            if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
            firstLine = false;
        }
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
        } else if (key == "demo") {
            cfg.demo = (value == "1" || value == "oui" || value == "true");
        } else if (key == "radio_url") {
            if (!value.empty()) {
                cfg.radioUrl = value;
            }
        } else if (key == "sun_time_scale") {
            try {
                cfg.sunTimeScale = std::stof(value);
            } catch (const std::exception&) {
                std::fprintf(stderr, "[Config] sun_time_scale invalide : %s\n", value.c_str());
            }
        } else {
            std::fprintf(stderr, "[Config] clé inconnue ignorée : %s\n", key.c_str());
        }
    }
    return cfg;
}

}  /* namespace artouste::app */
