/*
 * MapEntry.hpp
 * Une carte proposée au menu de démarrage.
 *
 * Peuplée par ApplicationMenuMaps.cpp, lue par runStartupMenu.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <string>

namespace artouste::app {

struct MapEntry {
    std::string dir;   /* nom du sous-dossier de assets/terrain (= nom du terrain) */
    std::string title; /* libellé lisible, tiré de la première ligne de terrain.txt */
    /* Présence de zombie_only.txt : carte dédiée au mode zombie (ex. dax-arene).
       Lancer normalement suffit à démarrer le combat. */
    bool zombieOnly = false;
};

/* Ce que le menu affiche pour cette carte. Le nom de dossier technique n'a pas
   sa place devant une arène dédiée au mode zombie ("Happy DeathHour") : titre
   seul pour celles-là. Les autres gardent le préfixe, utile pour retrouver le
   dossier de la clé "terrain" de config.txt. */
[[nodiscard]] inline std::string libelleCarte(const MapEntry& carte) {
    if (carte.zombieOnly) {
        return carte.title;
    }
    return carte.dir + "  -  " + carte.title;
}

} /* namespace artouste::app */
