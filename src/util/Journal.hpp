/*
 * Journal.hpp
 * Sorties de diagnostic sous Windows.
 *
 * L'exécutable est construit en sous-système graphique (WIN32_EXECUTABLE dans
 * src/CMakeLists.txt), donc le processus n'a pas de console, pas même celle du
 * terminal qui le lance : les printf du moteur sont perdus. Cette classe les
 * rend à la console appelante quand il y en a une, et les écrit toujours dans
 * %LOCALAPPDATA%\Artouste\artouste.log.
 *
 * Sous Linux et macOS elle ne fait rien : le terminal reçoit déjà tout.
 *
 * À construire en tête de main(), avant toute écriture. Sa destruction referme
 * le journal, y compris sur les sorties anticipées de main().
 *
 * Auteur : O. Booklage
 * Date : septembre 2026
 * Licence : GPL v2
 */

#pragma once

#include <filesystem>

namespace artouste::util {

class Journal {
public:
    Journal();
    ~Journal();

    Journal(const Journal&)            = delete;
    Journal& operator=(const Journal&) = delete;
    Journal(Journal&&)                 = delete;
    Journal& operator=(Journal&&)      = delete;

    /* Chemin du journal, vide s'il n'y en a pas (Linux, ou ouverture ratée). */
    [[nodiscard]] static std::filesystem::path chemin() noexcept;
};

} /* namespace artouste::util */
