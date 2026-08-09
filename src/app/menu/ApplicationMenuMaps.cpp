/*
 * ApplicationMenuMaps.cpp
 * Découverte et tri des cartes proposées au menu de démarrage : chaque
 * sous-dossier de assets/terrain contenant un terrain.txt, avec son titre
 * lisible et ses options (mode zombie). Consommé par runStartupMenu, dans
 * ApplicationMenu.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace artouste::app {

namespace {

/* Titre lisible d'une carte : première ligne de son terrain.txt, débarrassée du préfixe
   de commentaire "# Terrain Artouste - ". À défaut, on renvoie le nom du dossier. */
std::string lireTitreCarte(const std::filesystem::path& terrainTxt, const std::string& repli) {
    std::ifstream f(terrainTxt);
    std::string ligne;
    if (f && std::getline(f, ligne)) {
        const std::string prefixe = "# Terrain Artouste - ";
        if (const auto pos = ligne.find(prefixe); pos != std::string::npos) {
            return ligne.substr(pos + prefixe.size());
        }
        if (ligne.rfind("# ", 0) == 0) { /* commentaire sans le préfixe attendu */
            return ligne.substr(2);
        }
        if (!ligne.empty()) {
            return ligne;
        }
    }
    return repli;
}

} /* namespace */

std::vector<Application::MapEntry>
Application::recenserCartes(const std::filesystem::path& assets) {
    /* Trié par nom pour un ordre stable (la première est le choix par défaut), sauf
       les cartes dédiées au mode zombie (zombieOnly, ex. Happy DeathHour/dax-arene)
       qui passent systématiquement en dernier -- ce sont des arènes à part, pas des
       cartes de tourisme normales, et les mélanger dans le tri alphabétique n'aurait
       pas de sens. */
    namespace fs = std::filesystem;
    std::vector<MapEntry> cartes;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(assets / "terrain", ec)) {
        if (!e.is_directory()) {
            continue;
        }
        const fs::path txt = e.path() / "terrain.txt";
        if (fs::exists(txt)) {
            const std::string nom = e.path().filename().string();
            /* Présence de zombie_only.txt : même mécanisme que les autres fichiers
               optionnels par carte (exclusions.txt, hapi.txt) -- distingue une carte
               dédiée au mode zombie (dax-arene) d'une carte de tourisme normale. */
            const bool zombieOnly = fs::exists(e.path() / "zombie_only.txt");
            cartes.push_back({nom, lireTitreCarte(txt, nom), zombieOnly});
        }
    }
    std::sort(cartes.begin(), cartes.end(), [](const MapEntry& a, const MapEntry& b) {
        if (a.zombieOnly != b.zombieOnly) {
            return !a.zombieOnly; /* les cartes dédiées au mode zombie passent en dernier */
        }
        return a.dir < b.dir;
    });
    return cartes;
}

} /* namespace artouste::app */
