/*
 * Config.hpp
 * Configuration du simulateur, lue au lancement d'un fichier "clé valeur"
 * (assets/config.txt) éditable à la main. C'est le même format simple que le
 * calage du terrain (terrain.txt) : une clé et sa valeur par ligne, le caractère
 * # en début de ligne marque un commentaire. Toute clé absente garde sa valeur
 * par défaut, et un fichier manquant ne pose pas de problème.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include <filesystem>
#include <string>

namespace artouste::app {

struct Config {
    /* Nom du sous-dossier de assets/terrain/ à charger au lancement (par exemple
       "ossau" ou "cote-landes"). Chaque sous-dossier contient son propre
       terrain.txt, heightmap.png, ortho.jpg et landmarks.txt. */
    std::string terrain = "ossau";

    /* Démarrage immédiat : si vrai, la turbine et le rotor sont d'emblée au régime
       au lancement (au lieu de la séquence de démarrage). Pratique pour les tests,
       afin de pouvoir décoller tout de suite. */
    bool turbineRunning = false;
};

/* Lit la configuration depuis le fichier donné. Fichier absent ou clé inconnue :
   on garde les valeurs par défaut ci-dessus. */
Config loadConfig(const std::filesystem::path& path);

}  /* namespace artouste::app */
