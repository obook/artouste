/*
 * Config.cpp
 * Lecture du fichier de configuration "clé valeur" (voir Config.hpp).
 *
 * Les clés, le renommage des anciennes et le report des nouvelles vivent dans
 * app/config/.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Config.hpp"

#include "app/config/ConfigInterne.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <system_error>

namespace artouste::app {

Config loadConfig(const std::filesystem::path& path) {
    Config cfg;

    /* Options renommées depuis que ce fichier a été écrit : on leur rend leur nom
       actuel AVANT de lire, en gardant la valeur choisie. Le reste du chargement
       n'a ainsi jamais affaire qu'aux noms d'aujourd'hui. */
    renommerAnciennesCles(path, clesRenommees());

    std::ifstream in(path);
    if (!in) {
        /* Pas de config personnelle : on la crée en recopiant le modèle versionné
           config.default.txt (rangé à côté), comme un "config.dist" recopié au premier
           lancement. L'utilisateur édite ensuite sa copie, qui n'est pas suivie par git.
           Si le modèle manque aussi, on garde les valeurs par défaut, sans erreur. */
        const std::filesystem::path modele = path.parent_path() / "config.default.txt";
        std::error_code ec;
        if (std::filesystem::exists(modele, ec)) {
            std::filesystem::copy_file(modele, path, ec);
            if (!ec) {
                std::printf("[Config] %s absent : créé depuis %s\n",
                            path.filename().string().c_str(),
                            modele.filename().string().c_str());
                in.clear();
                in.open(path);
            } else {
                std::fprintf(stderr,
                             "[Config] création de %s impossible : %s\n",
                             path.filename().string().c_str(),
                             ec.message().c_str());
            }
        } else {
            /* Ni configuration personnelle ni modèle : le jeu vole sur ses
               valeurs par défaut, ce qui marche, mais l'utilisateur n'a aucun
               fichier à régler. Autant le dire. */
            std::fprintf(stderr,
                         "[Config] ni %s ni %s : le simulateur part "
                         "sur ses valeurs par défaut.\n",
                         path.filename().string().c_str(),
                         modele.filename().string().c_str());
        }
        if (!in) {
            return cfg;
        }
    }

    /* Clés rencontrées dans le fichier de l'utilisateur, y compris celles que ce
       chargeur ne connaît pas : elles servent à repérer, en fin de lecture, les
       options du modèle qui manquent ici (voir completerDepuisModele). */
    std::set<std::string> clesPresentes;

    std::string line;
    bool firstLine = true;
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
            continue; /* ligne vide ou commentaire */
        }
        std::istringstream iss(clean);
        std::string key;
        iss >> key;
        std::string value;
        std::getline(iss, value);
        value = trim(value);
        clesPresentes.insert(key);

        if (key == "terrain") {
            if (!value.empty()) {
                cfg.terrain = value;
            }
        } else if (key == "turbine_demarree") {
            cfg.turbineRunning = (value == "1" || value == "oui" || value == "true");
        } else if (key == "demo") {
            cfg.demo = (value == "1" || value == "oui" || value == "true");
        } else if (key == "arbres") {
            /* Défaut à vrai : seule une valeur explicitement négative désactive les
               arbres (toute autre valeur, dont "1"/"oui"/"true", les garde). */
            cfg.trees = !(value == "0" || value == "non" || value == "false");
        } else if (key == "relief_fenetre") {
            /* Défaut à faux : seule une valeur explicitement positive allume la
               fenêtre de relief (logique inverse de "arbres"). */
            cfg.reliefWindow = (value == "1" || value == "oui" || value == "true");
        } else if (key == "relief_debug") {
            /* Défaut à faux, comme la fenêtre dont elle trace les frontières. */
            cfg.reliefDebug = (value == "1" || value == "oui" || value == "true");
        } else if (key == "zone_hv") {
            /* Défaut à faux : seule une valeur explicitement positive allume
               l'indicateur (logique inverse de "arbres"). */
            cfg.zoneHv = (value == "1" || value == "oui" || value == "true");
        } else if (key == "verifier_maj") {
            /* Défaut à vrai : seule une valeur explicitement négative coupe la
               recherche de mise à jour (même logique que "arbres"). */
            cfg.checkUpdate = !(value == "0" || value == "non" || value == "false");
        } else if (key == "radio_url") {
            if (!value.empty()) {
                cfg.radioUrl = value;
            }
        } else if (key == "soleil_vitesse") {
            try {
                cfg.sunTimeScale = std::stof(value);
            } catch (const std::exception&) {
                std::fprintf(stderr, "[Config] soleil_vitesse invalide : %s\n", value.c_str());
            }
        } else if (key == "brume_debut" || key == "brume_fin") {
            try {
                /* Bornes larges mais non absurdes : une brume qui commence à zéro
                   noierait le cockpit, une fin plus courte que le début inverserait
                   le fondu. La cohérence des deux est vérifiée après la lecture. */
                const float metres = std::clamp(std::stof(value), 100.0f, 100000.0f);
                (key == "brume_debut" ? cfg.fogStartM : cfg.fogEndM) = metres;
            } catch (const std::exception&) {
                std::fprintf(stderr, "[Config] %s invalide : %s\n", key.c_str(), value.c_str());
            }
        } else if (key == "lune_vitesse") {
            try {
                /* Borné par le bas : une valeur nulle ou négative arrêterait la nuit
                   pour toujours, l'appareil restant dans le noir sans que le pilote
                   comprenne d'où vient la panne. */
                cfg.nightSpeedFactor = std::clamp(std::stof(value), 0.1f, 100.0f);
            } catch (const std::exception&) {
                std::fprintf(stderr, "[Config] lune_vitesse invalide : %s\n", value.c_str());
            }
        } else if (key == "arbres_max") {
            try {
                cfg.treeBudget = std::max(0, std::stoi(value));
            } catch (const std::exception&) {
                std::fprintf(stderr, "[Config] arbres_max invalide : %s\n", value.c_str());
            }
        } else if (key == "tuiles_fenetre_px") {
            try {
                /* Borne haute large : au-delà de 16384 px, aucune carte graphique
                   grand public n'accepterait la texture. */
                cfg.detailWindowPx = std::clamp(std::stoi(value), 0, 16384);
            } catch (const std::exception&) {
                std::fprintf(stderr, "[Config] tuiles_fenetre_px invalide : %s\n", value.c_str());
            }
        } else if (key == "tuiles_dossier") {
            if (!value.empty()) {
                cfg.tilesDir = value;
            }
        } else if (key == "relief_sommets_max") {
            try {
                cfg.reliefVertexBudget = std::max(0, std::stoi(value));
            } catch (const std::exception&) {
                std::fprintf(stderr, "[Config] relief_sommets_max invalide : %s\n", value.c_str());
            }
        } else if (key == "msaa") {
            try {
                /* Bornes GLFW usuelles : 0, 2, 4, 8. On borne au cas où. */
                cfg.msaa = std::clamp(std::stoi(value), 0, 16);
            } catch (const std::exception&) {
                std::fprintf(stderr, "[Config] msaa invalide : %s\n", value.c_str());
            }
        } else if (clesRetirees().count(key) != 0) {
            /* Option supprimée depuis : le fichier de l'utilisateur la porte encore,
               on la passe SANS RIEN DIRE. La signaler comme inconnue ferait croire à
               un défaut alors que c'est nous qui l'avons retirée. */
        } else {
            std::fprintf(stderr, "[Config] clé inconnue ignorée : %s\n", key.c_str());
        }
    }
    in.close();

    /* Brume à l'envers (fin avant début) : le fondu s'inverserait, le lointain
       redevenant net et le proche disparaissant. On rétablit un ordre valide
       plutôt que d'ignorer les deux valeurs, l'intention restant lisible. */
    if (cfg.fogEndM <= cfg.fogStartM) {
        std::fprintf(stderr,
                     "[Config] brume_fin (%.0f) doit dépasser brume_debut (%.0f) : "
                     "fin portée à %.0f.\n",
                     static_cast<double>(cfg.fogEndM),
                     static_cast<double>(cfg.fogStartM),
                     static_cast<double>(cfg.fogStartM * 2.0f));
        cfg.fogEndM = cfg.fogStartM * 2.0f;
    }

    /* Fichier écrit par une version plus ancienne : on y ajoute les options
       apparues depuis, pour que l'utilisateur les voie et puisse les régler. Les
       valeurs recopiées étant celles du modèle, la configuration que l'on vient
       de lire reste juste pour ce lancement-ci. */
    completerDepuisModele(path, path.parent_path() / "config.default.txt", clesPresentes);
    return cfg;
}

} /* namespace artouste::app */
