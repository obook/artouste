/*
 * aide_config.hpp
 * Fichiers temporaires et lecture du modèle, partagés par les tests de
 * configuration.
 *
 * Chaque essai a son propre dossier : le chargeur écrit à côté du fichier
 * qu'on lui donne, deux essais ne doivent pas se marcher dessus.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/Config.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>

namespace essais_config {

/* Écrit un fichier de configuration temporaire et renvoie son chemin. Chaque
   appel a son propre dossier : le chargeur écrit à côté du fichier qu'on lui
   donne (modèle réécrit, options ajoutées), et deux essais ne doivent pas se
   marcher dessus -- ni salir le dossier temporaire du système. */
inline std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const std::filesystem::path dossier = std::filesystem::temp_directory_path() / (name + ".d");
    std::filesystem::remove_all(dossier);
    std::filesystem::create_directories(dossier);
    const std::filesystem::path path = dossier / "config.txt";
    /* Mode binaire : sans lui, Windows traduit chaque \n en \r\n à l'écriture, et
       le fichier ne contient plus les octets que le test croit y avoir mis. Les
       essais sur les fins de ligne, eux, doivent poser exactement ce qu'ils
       annoncent. */
    std::ofstream out(path, std::ios::binary);
    out << content;
    return path;
}

/* Efface le dossier d'un fichier rendu par writeTemp. */
inline void effacerTemp(const std::filesystem::path& path) {
    std::filesystem::remove_all(path.parent_path());
}


/* Lit un fichier en entier, pour vérifier ce que le chargeur y a écrit. */
inline std::string lireTout(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/* Prépare un dossier de test contenant un modèle config.default.txt et une
   configuration personnelle incomplète, comme celle d'un utilisateur ayant
   installé une version plus ancienne. */
inline std::filesystem::path preparerDossier(const std::string& nom,
                                      const std::string& configPersonnelle) {
    const std::filesystem::path dossier = std::filesystem::temp_directory_path() / nom;
    std::filesystem::remove_all(dossier);
    std::filesystem::create_directories(dossier);
    std::ofstream modele(dossier / "config.default.txt", std::ios::binary);
    modele << "# terrain : carte chargée au lancement.\n"
              "terrain ossau\n"
              "\n"
              "# verifier_maj : recherche d'une mise à jour au lancement.\n"
              "# 0 pour ne rien demander au réseau.\n"
              "verifier_maj 1\n";
    modele.close();
    std::ofstream perso(dossier / "config.txt", std::ios::binary);
    perso << configPersonnelle;
    return dossier;
}

/* Clés portées par le vrai modèle livré avec le jeu (assets/config.default.txt),
   dont le chemin est passé par CMake. */
inline std::set<std::string> clesDuModele() {
    std::set<std::string> cles;
    std::ifstream in(ARTOUSTE_CONFIG_MODELE);
    REQUIRE(in); /* modèle introuvable : c'est le test lui-même qui est mal réglé */
    std::string ligne;
    while (std::getline(in, ligne)) {
        if (!ligne.empty() && ligne.back() == '\r') {
            ligne.pop_back();
        }
        const std::size_t debut = ligne.find_first_not_of(" \t");
        if (debut == std::string::npos || ligne[debut] == '#') {
            continue;
        }
        std::istringstream iss(ligne);
        std::string cle;
        iss >> cle;
        if (!cle.empty()) {
            cles.insert(cle);
        }
    }
    return cles;
}

} /* namespace essais_config */
