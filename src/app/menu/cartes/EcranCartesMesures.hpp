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
#include <functional>
#include <string>

namespace artouste::app::ecran_cartes {

/* Appelé de loin en loin PENDANT une marche de dossier, pour que l'écran
   d'attente reste vivant : ces mesures durent des secondes sur un jeu de
   tuiles, et rien ne les découpe de l'extérieur. À l'appelant de se brider,
   chaque image attendant la synchronisation verticale. */
using Battement = std::function<void()>;

/* Taille cumulée d'un dossier. Des milliers de fichiers sur un jeu de tuiles :
   à appeler à l'ouverture de l'écran, jamais à chaque image. */
[[nodiscard]] std::uintmax_t tailleDossier(const std::filesystem::path& dossier,
                                           const Battement&             battre = {});

/* Tuiles posées sous un niveau, sans descendre aux niveaux plus fins.
   L'extension distingue les deux jeux : .dds pour l'image, .r16 pour le
   relief. */
[[nodiscard]] int compterTuiles(const std::filesystem::path& niveau,
                                const char*                  extension = ".dds",
                                const Battement&             battre    = {});

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
