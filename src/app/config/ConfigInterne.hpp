/*
 * ConfigInterne.hpp
 * Ce que les fichiers du chargeur de configuration se passent entre eux et qui
 * n'a pas à sortir de app/config/.
 *
 * L'interface publique (loadConfig, clesConnues, clesRenommees,
 * renommerAnciennesCles) reste dans app/Config.hpp.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <filesystem>
#include <set>
#include <string>

namespace artouste::app {

/* Retire les espaces de début et de fin. La valeur est le reste de la ligne
   après la clé : sans cela elle traînerait ses blancs. */
[[nodiscard]] std::string trim(const std::string& s);

/* Première clé d'une ligne déjà nettoyée, vide si la ligne n'en porte pas. */
[[nodiscard]] std::string cleDeLigne(const std::string& ligneNettoyee);

/* Options qui ont existé puis ont été retirées. On les passe en silence : les
   signaler comme inconnues ferait croire à un défaut de l'utilisateur. */
[[nodiscard]] const std::set<std::string>& clesRetirees();

/* Ajoute au config.txt les options du modèle qui lui manquent, documentation
   comprise. Rien n'est réécrit : on n'ajoute qu'à la fin. */
void completerDepuisModele(const std::filesystem::path& config,
                           const std::filesystem::path& modele,
                           const std::set<std::string>& clesPresentes);

} /* namespace artouste::app */
