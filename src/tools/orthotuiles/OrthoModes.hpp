/*
 * OrthoModes.hpp
 * Les trois façons de lancer l'outil : écrire l'index seul, remplir un bloc,
 * découper toute la carte.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "tools/orthotuiles/OrthoCalage.hpp"
#include "tools/orthotuiles/OrthoComposition.hpp"

#include <filesystem>

namespace orthotuiles {

/* Ce que la ligne de commande a demandé. */
struct Options {
    std::filesystem::path dossierCarte;
    std::filesystem::path dossierSortie;
    std::filesystem::path source;
    float                 mParPixelVoulu = 0.0f;
    int                   tuilePx        = 512;
    bool                  reprendre      = false;
    bool                  indexSeul      = false;
    int                   apercuCol      = -1;
    int                   apercuRangee   = -1;
    std::filesystem::path apercuFichier;
    int                   blocCol    = -1;
    int                   blocRangee = -1;
};

/* Chaque mode renvoie le code de sortie du programme. */
int modeIndexSeul(const Options& opt, const CalageCarte& carte);
int modeBloc(const Options& opt, const Source& src);
int modeApercu(const Options& opt, const CalageCarte& carte, const Source& src, float mParPixel);
int modeComplet(const Options& opt, const CalageCarte& carte, const Source& src, float mParPixel);

} /* namespace orthotuiles */
