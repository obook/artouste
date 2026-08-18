/*
 * ConfigTexte.cpp
 * Découpage d'une ligne "clé valeur" (voir ConfigInterne.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/config/ConfigInterne.hpp"

#include <sstream>

namespace artouste::app {

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

} /* namespace artouste::app */
