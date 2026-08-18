/*
 * EcranCartesMesures.cpp
 * Mesures du disque et mises en forme lisibles du gestionnaire de cartes
 * (voir EcranCartesMesures.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/menu/cartes/EcranCartesMesures.hpp"

#include <cmath>
#include <cstdio>
#include <system_error>

namespace artouste::app::ecran_cartes {

[[nodiscard]] std::uintmax_t tailleDossier(const std::filesystem::path& dossier) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dossier, ec)) {
        return 0;
    }
    std::uintmax_t total = 0;
    for (const auto& entree :
         std::filesystem::recursive_directory_iterator(dossier, ec)) {
        if (entree.is_regular_file(ec)) {
            total += entree.file_size(ec);
        }
    }
    return total;
}

/* Un seul niveau : un compte récursif ramasserait les niveaux plus fins rangés
   en dessous, et on comparerait deux grilles à l'attendu d'une seule. */
[[nodiscard]] int compterTuiles(const std::filesystem::path& niveau) {
    std::error_code ec;
    int             tuiles = 0;
    for (const auto& rangee : std::filesystem::directory_iterator(niveau, ec)) {
        if (!rangee.is_directory(ec)) {
            continue;
        }
        for (const auto& fichier : std::filesystem::directory_iterator(rangee.path(), ec)) {
            if (fichier.path().extension() == ".dds") {
                ++tuiles;
            }
        }
    }
    return tuiles;
}

[[nodiscard]] std::uintmax_t tailleFichier(const std::filesystem::path& fichier) {
    std::error_code ec;
    const auto      taille = std::filesystem::file_size(fichier, ec);
    return ec ? 0 : taille;
}

[[nodiscard]] std::string formaterOctets(std::uintmax_t octets) {
    char tampon[32];
    if (octets >= 1000ull * 1000ull * 1000ull) {
        std::snprintf(tampon, sizeof(tampon), "%.1f Go", static_cast<double>(octets) / 1e9);
    } else if (octets >= 1000ull * 1000ull) {
        std::snprintf(tampon, sizeof(tampon), "%.0f Mo", static_cast<double>(octets) / 1e6);
    } else if (octets > 0) {
        std::snprintf(tampon, sizeof(tampon), "%.0f Ko", static_cast<double>(octets) / 1e3);
    } else {
        std::snprintf(tampon, sizeof(tampon), "-");
    }
    return tampon;
}

/* On descend en kilooctets sous le mégaoctet : 205 Ko/s s'affichait "0,2 Mo/s",
   où les chiffres utiles disparaissent. */
[[nodiscard]] std::string formaterDebit(double octetsParSeconde) {
    char tampon[32];
    if (octetsParSeconde >= 1e6) {
        std::snprintf(tampon, sizeof(tampon), "%.1f Mo/s", octetsParSeconde / 1e6);
    } else {
        std::snprintf(tampon, sizeof(tampon), "%.0f Ko/s", octetsParSeconde / 1e3);
    }
    return tampon;
}

/* Quatre positions ASCII : la police ImGui chargée ici s'arrête au latin-1, un
   braille tournant n'y donnerait qu'un carré vide. */
[[nodiscard]] char caractereTournant(double secondes) {
    static constexpr char PHASES[] = {'|', '/', '-', '\\'};
    const int             phase    = static_cast<int>(secondes * 8.0) % 4;
    return PHASES[phase < 0 ? 0 : phase];
}

/* L'arrondi précède le choix de l'unité, sans quoi 59,7 minutes s'afficheraient
   "60 minutes". */
[[nodiscard]] std::string formaterDuree(double secondes) {
    char         tampon[32];
    const double minutes = std::round(secondes / 60.0);
    if (minutes >= 60.0) {
        std::snprintf(tampon, sizeof(tampon), "%.0f h %02.0f", std::floor(minutes / 60.0),
                      minutes - 60.0 * std::floor(minutes / 60.0));
    } else {
        std::snprintf(tampon, sizeof(tampon), "%.0f minutes", minutes);
    }
    return tampon;
}

} /* namespace artouste::app::ecran_cartes */
