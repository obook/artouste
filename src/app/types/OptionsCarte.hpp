/*
 * OptionsCarte.hpp
 * Options d'une carte (options.txt facultatif) combinées à la configuration
 * générale : ce que le moteur applique réellement à cette carte-là.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::app {

struct OptionsCarte {
    bool arbres    = true;
    bool batiments = true;
    bool tuiles    = true;

    [[nodiscard]] bool operator==(const OptionsCarte&) const = default;
};

} /* namespace artouste::app */
