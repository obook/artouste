/*
 * Config.cpp
 * Lecture du fichier de configuration "clé valeur" (voir Config.hpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Config.hpp"

#include "app/ConfigModele.hpp" /* copie du modèle fabriquée par CMake */

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

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

/* Première clé d'une ligne déjà nettoyée, ou chaîne vide si la ligne n'en porte
   pas (ligne vide ou commentaire). */
std::string cleDeLigne(const std::string& ligneNettoyee) {
    if (ligneNettoyee.empty() || ligneNettoyee[0] == '#') {
        return "";
    }
    std::istringstream iss(ligneNettoyee);
    std::string cle;
    iss >> cle;
    return cle;
}

/* Ajoute à la fin de la configuration personnelle les options que le modèle
   connaît et qu'elle n'a pas. C'est le cas d'un fichier écrit par une version
   plus ancienne du simulateur : sans cela, une option nouvelle resterait à
   jamais invisible pour qui a déjà son config.txt, puisque le modèle n'est
   recopié qu'à la toute première exécution.

   Chaque option manquante est recopiée du modèle avec les commentaires qui la
   documentent, et avec la valeur qu'elle y porte -- celle que reçoit une
   installation neuve. Rien n'est réécrit : on n'ajoute qu'à la fin, les
   réglages existants ne sont jamais touchés.

   Le modèle est un simple fichier texte, posé à côté du jeu : l'utilisateur a
   pu l'effacer, l'écraser ou le remplir de n'importe quoi. On ne lui accorde donc
   aucune confiance : seules les options que le chargeur sait vraiment lire
   (clesConnues) sont recopiées, et tout le reste est signalé puis ignoré. Sans
   ce filtre, un modèle abîmé écrirait ses lignes parasites -- jusqu'à des octets
   binaires -- dans la configuration personnelle, qui n'a rien demandé.

   Le modèle du disque peut avoir été effacé ou écrasé par autre chose. Le
   simulateur en porte une copie, faite à la compilation depuis le même fichier
   (ConfigModele.hpp) : elle sert de secours quand celui du disque est illisible,
   pour que les options nouvelles parviennent quand même à l'utilisateur. Un
   modèle présent et lisible, lui, est toujours préféré -- il a pu être adapté
   volontairement, et ce n'est pas au jeu d'en décider.

   Si la configuration ne peut pas être écrite (jeu installé en lecture seule),
   on se contente de prévenir : ce n'est pas une erreur, la configuration lue
   reste valable et le vol part normalement. */
void completerDepuisModele(const std::filesystem::path& config,
                           const std::filesystem::path& modele,
                           const std::set<std::string>& clesPresentes) {
    std::string texte;
    {
        std::ifstream in(modele, std::ios::binary);
        texte.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        if (!in) {
            std::fprintf(stderr,
                         "[Config] %s illisible : les options nouvelles sont prises dans la "
                         "copie embarquée dans le simulateur.\n",
                         modele.filename().string().c_str());
            texte = MODELE_CONFIG_INTEGRE;
        }
    }
    /* Un octet de contrôle (hors tabulation et fins de ligne) ne se trouve pas
       dans un fichier texte "clé valeur" : le modèle a été écrasé par autre
       chose, un binaire ou un fichier tronqué. On n'en tire alors RIEN, pas même
       ses lignes d'apparence correcte -- une valeur absurde venue d'un tel
       fichier (msaa 999...) n'a pas plus de raison d'entrer dans la
       configuration personnelle que le reste -- et on se rabat sur la copie
       embarquée, elle toujours saine. */
    for (const char c : texte) {
        const unsigned char octet = static_cast<unsigned char>(c);
        if (octet < 0x20 && c != '\t' && c != '\n' && c != '\r') {
            std::fprintf(stderr,
                         "[Config] %s abîmé (caractères binaires) : il est ignoré, les options "
                         "nouvelles sont prises dans la copie embarquée dans le simulateur.\n",
                         modele.filename().string().c_str());
            texte = MODELE_CONFIG_INTEGRE;
            break;
        }
    }

    std::vector<std::string> commentaires; /* lignes # qui précèdent la clé en cours */
    std::vector<std::string> ajoutees;     /* clés effectivement recopiées */
    std::vector<std::string> etrangeres;   /* clés du modèle que le jeu ne connaît pas */
    std::string blocs;                     /* texte à ajouter en fin de fichier */
    std::string ligne;
    std::istringstream flux(texte);
    while (std::getline(flux, ligne)) {
        if (!ligne.empty() && ligne.back() == '\r') {
            ligne.pop_back(); /* modèle enregistré en CRLF */
        }
        const std::string clean = trim(ligne);
        if (clean.empty()) {
            commentaires.clear(); /* une ligne vide ferme le bloc en cours */
            continue;
        }
        if (clean[0] == '#') {
            commentaires.push_back(ligne);
            continue;
        }
        const std::string cle = cleDeLigne(clean);
        /* Les commentaires ne valent que pour la clé qui suit : on les emporte ici
           et on remet le compteur à zéro pour le bloc suivant, quel que soit le
           sort de cette clé-ci. */
        const std::vector<std::string> documentation = commentaires;
        commentaires.clear();
        if (cle.empty()) {
            continue;
        }
        if (clesConnues().find(cle) == clesConnues().end()) {
            /* Le modèle décrit une option que ce jeu ne sait pas lire : modèle
               abîmé, ou venu d'une autre version. On la laisse où elle est. */
            etrangeres.push_back(cle);
            continue;
        }
        if (clesPresentes.find(cle) != clesPresentes.end()) {
            continue; /* l'utilisateur a déjà cette option, on n'y touche pas */
        }
        blocs += "\n";
        for (const std::string& commentaire : documentation) {
            blocs += commentaire + "\n";
        }
        blocs += ligne + "\n";
        ajoutees.push_back(cle);
    }
    if (!etrangeres.empty()) {
        std::string liste;
        for (const std::string& cle : etrangeres) {
            if (!liste.empty()) {
                liste += ", ";
            }
            /* Une clé venue d'un fichier abîmé peut contenir n'importe quoi : on
               n'en affiche qu'un extrait, et rien qui ne soit imprimable. */
            for (std::size_t i = 0; i < cle.size() && i < 24; ++i) {
                const unsigned char c = static_cast<unsigned char>(cle[i]);
                liste += (c >= 0x20 && c != 0x7F) ? cle[i] : '?';
            }
        }
        std::fprintf(stderr,
                     "[Config] %s : option(s) inconnue(s) de ce simulateur, non recopiée(s) "
                     "(modèle abîmé ou d'une autre version) : %s\n",
                     modele.filename().string().c_str(),
                     liste.c_str());
    }
    if (ajoutees.empty()) {
        return; /* configuration déjà complète : cas normal, on n'ouvre rien en écriture */
    }

    /* Fins de ligne du fichier de l'utilisateur : un fichier enregistré par le
       Bloc-notes de Windows est en CRLF, et y mêler des lignes en LF donnerait un
       fichier bâtard, illisible dans certains éditeurs. On lit donc l'existant tel
       quel (binaire) pour reprendre sa convention, et pour savoir s'il se termine
       bien par un saut de ligne. */
    std::string existant;
    {
        std::ifstream lecture(config, std::ios::binary);
        existant.assign(std::istreambuf_iterator<char>(lecture), std::istreambuf_iterator<char>());
    }
    const bool crlf = existant.find("\r\n") != std::string::npos;

    std::string ajout =
        "\n"
        "# ==========================================================================\n"
        "# Options apparues depuis que ce fichier a été créé, recopiées du modèle\n"
        "# config.default.txt telles qu'une installation neuve les reçoit. Modifiez-les,\n"
        "# déplacez-les ou commentez-les à votre guise ; une option effacée sera\n"
        "# simplement réajoutée ici au prochain lancement.\n"
        "# ==========================================================================\n" +
        blocs;
    if (crlf) {
        std::string converti;
        converti.reserve(ajout.size() + ajout.size() / 8);
        for (const char c : ajout) {
            if (c == '\n') {
                converti += '\r';
            }
            converti += c;
        }
        ajout.swap(converti);
    }
    if (!existant.empty() && existant.back() != '\n') {
        ajout.insert(0, crlf ? "\r\n" : "\n"); /* le fichier ne finissait pas par un saut */
    }

    std::ofstream out(config, std::ios::binary | std::ios::app);
    if (!out) {
        std::fprintf(stderr,
                     "[Config] %s non modifiable : les nouvelles options n'y ont pas été "
                     "ajoutées (elles gardent leur valeur par défaut).\n",
                     config.filename().string().c_str());
        return;
    }
    out << ajout;

    std::string liste;
    for (const std::string& cle : ajoutees) {
        if (!liste.empty()) {
            liste += ", ";
        }
        liste += cle;
    }
    std::printf("[Config] nouvelle(s) option(s) ajoutée(s) à %s : %s\n",
                config.filename().string().c_str(),
                liste.c_str());
}

} /* namespace */

const std::set<std::string>& clesConnues() {
    /* Ordre sans importance : c'est un ensemble. Toute clé ajoutée au chargeur
       ci-dessous doit figurer ici ET dans assets/config.default.txt, faute de quoi
       les tests de cohérence échouent (voir tests/config_tests.cpp). */
    static const std::set<std::string> cles = {"terrain",
                                               "turbine_demarree",
                                               "demo",
                                               "arbres",
                                               "souffle",
                                               "verifier_maj",
                                               "radio_url",
                                               "soleil_vitesse",
                                               "lune_vitesse",
                                               "arbres_max",
                                               "tuiles_fenetre_px",
                                               "tuiles_dossier",
                                               "relief_sommets_max",
                                               "msaa"};
    return cles;
}

const std::map<std::string, std::string>& clesRenommees() {
    /* Ancien nom -> nom actuel. Vide tant qu'aucune option n'a été renommée ; une
       entrée ajoutée ici ne s'en retire jamais, car il existera toujours quelque
       part un config.txt d'une version d'avant. Le nom actuel doit figurer dans
       clesConnues(), l'ancien n'y figure plus (les tests le vérifient). */
    static const std::map<std::string, std::string> renommages = {
        /* Juillet 2026 : clés uniformisées en français. */
        {"tree_max", "arbres_max"},
        {"sun_time_scale", "soleil_vitesse"},
    };
    return renommages;
}

std::size_t renommerAnciennesCles(const std::filesystem::path& config,
                                  const std::map<std::string, std::string>& renommages) {
    if (renommages.empty()) {
        return 0; /* cas courant : aucune option n'a jamais été renommée */
    }
    std::string contenu;
    {
        std::ifstream in(config, std::ios::binary);
        if (!in) {
            return 0;
        }
        contenu.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    /* Découpage en lignes, fins de ligne comprises : tout ce qui n'est pas une clé
       à renommer doit ressortir octet pour octet, commentaires et présentation de
       l'utilisateur compris. */
    std::vector<std::string> lignes;
    for (std::size_t debut = 0; debut < contenu.size();) {
        const std::size_t saut = contenu.find('\n', debut);
        const std::size_t fin = (saut == std::string::npos) ? contenu.size() : saut + 1;
        lignes.push_back(contenu.substr(debut, fin - debut));
        debut = fin;
    }

    /* Clés déjà présentes : si le fichier porte déjà le nom actuel, renommer
       l'ancienne ligne créerait un doublon, et la dernière valeur lue l'emporterait
       sans que personne comprenne pourquoi. */
    std::set<std::string> presentes;
    for (const std::string& ligne : lignes) {
        const std::string cle = cleDeLigne(trim(ligne));
        if (!cle.empty()) {
            presentes.insert(cle);
        }
    }

    const bool crlf = contenu.find("\r\n") != std::string::npos;
    const char* fdl = crlf ? "\r\n" : "\n";

    std::size_t modifiees = 0;
    std::vector<std::string> sortie;
    sortie.reserve(lignes.size() + 4);
    for (const std::string& ligne : lignes) {
        const std::size_t debut = ligne.find_first_not_of(" \t");
        if (debut == std::string::npos || ligne[debut] == '#') {
            sortie.push_back(ligne);
            continue;
        }
        std::size_t fin = ligne.find_first_of(" \t\r\n", debut);
        if (fin == std::string::npos) {
            fin = ligne.size();
        }
        const std::string cle = ligne.substr(debut, fin - debut);
        const auto trouv = renommages.find(cle);
        if (trouv == renommages.end()) {
            sortie.push_back(ligne);
            continue;
        }
        if (presentes.find(trouv->second) != presentes.end()) {
            /* Le nom actuel est déjà là : on neutralise l'ancienne ligne au lieu
               d'en faire un doublon, en expliquant pourquoi juste au-dessus. */
            sortie.push_back(std::string("# Ancien nom de \"") + trouv->second +
                             "\", que ce fichier porte déjà : ligne neutralisée par le "
                             "simulateur." +
                             fdl);
            sortie.push_back("# " + ligne);
        } else {
            /* Renommage : seul le nom change, la valeur et la mise en page restent
               exactement ce que l'utilisateur avait écrit. */
            sortie.push_back(ligne.substr(0, debut) + trouv->second + ligne.substr(fin));
            /* Le nom actuel existe désormais dans le fichier : si deux anciens noms
               menaient au même, le second sera neutralisé, pas dupliqué. */
            presentes.insert(trouv->second);
        }
        ++modifiees;
        std::printf("[Config] option renommée dans %s : %s -> %s\n",
                    config.filename().string().c_str(),
                    cle.c_str(),
                    trouv->second.c_str());
    }
    if (modifiees == 0) {
        return 0; /* rien à changer : on ne touche pas au fichier */
    }

    /* Réécriture par fichier intermédiaire puis remplacement : contrairement au
       complètement, qui n'ajoute qu'à la fin, on réécrit ici tout le fichier. Une
       coupure de courant ou un disque plein au mauvais moment ne doit pas laisser
       l'utilisateur avec une configuration tronquée. */
    const std::filesystem::path temporaire = config.string() + ".nouveau";
    {
        std::ofstream out(temporaire, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::fprintf(stderr,
                         "[Config] %s non modifiable : les options renommées gardent leur ancien "
                         "nom et seront ignorées.\n",
                         config.filename().string().c_str());
            return 0;
        }
        for (const std::string& ligne : sortie) {
            out << ligne;
        }
        if (!out) {
            std::fprintf(stderr,
                         "[Config] écriture de %s incomplète : fichier d'origine gardé.\n",
                         temporaire.filename().string().c_str());
            out.close();
            std::error_code ec;
            std::filesystem::remove(temporaire, ec);
            return 0;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporaire, config, ec);
    if (ec) {
        std::fprintf(stderr,
                     "[Config] remplacement de %s impossible (%s) : fichier d'origine gardé.\n",
                     config.filename().string().c_str(),
                     ec.message().c_str());
        std::filesystem::remove(temporaire, ec);
        return 0;
    }
    return modifiees;
}

Config loadConfig(const std::filesystem::path& path) {
    Config cfg;

    /* Modèle effacé : on le réécrit depuis la copie que le simulateur porte en
       lui (ConfigModele.hpp, fabriquée à la compilation depuis ce même fichier).
       Sans cela, l'utilisateur perdait pour de bon la documentation des options
       et le mécanisme qui lui apporte les nouvelles ; il fallait réinstaller. Un
       modèle présent n'est jamais réécrit : il a pu être adapté volontairement. */
    {
        const std::filesystem::path modele = path.parent_path() / "config.default.txt";
        std::error_code ec;
        if (!std::filesystem::exists(modele, ec)) {
            std::ofstream sortie(modele, std::ios::binary);
            if (sortie) {
                sortie << MODELE_CONFIG_INTEGRE;
            }
            if (sortie) {
                std::printf("[Config] %s manquant : réécrit depuis la copie du simulateur.\n",
                            modele.filename().string().c_str());
            } else {
                std::fprintf(stderr,
                             "[Config] %s manquant et non réécrit (dossier en lecture seule ?) : "
                             "le simulateur se rabat sur sa copie interne.\n",
                             modele.filename().string().c_str());
            }
        }
    }

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
            /* Le modèle vient pourtant d'être réécrit plus haut : s'il manque
               encore, c'est que le dossier n'accepte pas l'écriture. Le jeu vole
               alors sur ses valeurs par défaut, ce qui marche, mais l'utilisateur
               n'a aucun fichier à régler. Autant le dire. */
            std::fprintf(stderr,
                         "[Config] ni %s ni %s, et dossier non modifiable : le simulateur part "
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
        } else if (key == "souffle") {
            /* Défaut à vrai, même logique que "arbres" : seule une valeur
               explicitement négative coupe la poussière du souffle rotor. */
            cfg.rotorWash = !(value == "0" || value == "non" || value == "false");
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
        } else {
            std::fprintf(stderr, "[Config] clé inconnue ignorée : %s\n", key.c_str());
        }
    }
    in.close();

    /* Fichier écrit par une version plus ancienne : on y ajoute les options
       apparues depuis, pour que l'utilisateur les voie et puisse les régler. Les
       valeurs recopiées étant celles du modèle, la configuration que l'on vient
       de lire reste juste pour ce lancement-ci. */
    completerDepuisModele(path, path.parent_path() / "config.default.txt", clesPresentes);
    return cfg;
}

} /* namespace artouste::app */
