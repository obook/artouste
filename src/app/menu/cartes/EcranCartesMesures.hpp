/*
 * EcranCartesMesures.hpp
 * Peser un dossier, et mettre octets, débit et durée en forme lisible.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace artouste::app::ecran_cartes {

/* Taille cumulée d'un dossier. Des milliers de fichiers sur un jeu de tuiles :
   à appeler à l'ouverture de l'écran, jamais à chaque image. */
[[nodiscard]] std::uintmax_t tailleDossier(const std::filesystem::path& dossier);

/* Tuiles posées sous un niveau, sans descendre aux niveaux plus fins. */
[[nodiscard]] int compterTuiles(const std::filesystem::path& niveau);

/* Taille d'un fichier, 0 s'il est absent ou illisible. */
[[nodiscard]] std::uintmax_t tailleFichier(const std::filesystem::path& fichier);

/* Ko, Mo, Go. On ne montre jamais des octets bruts. */
[[nodiscard]] std::string formaterOctets(std::uintmax_t octets);

/* Ko/s ou Mo/s. */
[[nodiscard]] std::string formaterDebit(double octetsParSeconde);

/* Tourniquet d'attente : dit que la fabrication n'est pas figée. */
[[nodiscard]] char caractereTournant(double secondes);

/* Minutes, puis heures au-delà de soixante. */
[[nodiscard]] std::string formaterDuree(double secondes);

} /* namespace artouste::app::ecran_cartes */
